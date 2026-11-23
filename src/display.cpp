// display.cpp - 1.3" IPS screen 240x240 (ST7789, module GMT130-V1.0, no CS pin)
//
// Screen layout (pixels):
//   y   0..23   plant name          (redrawn only when it changes)
//   y  24..215  flower scene        (new frame every DISPLAY_FRAME_MS)
//   y 216..239  data line / error   (redrawn only when it changes)
//
// The scene is drawn into a small canvas 120x96 in RAM and sent to the
// screen scaled x2 (each canvas pixel becomes 2x2 screen pixels).
// A full size canvas 240x192 would need 92 KB RAM, this one needs 23 KB
// (the HTTPS keep-alive connection already holds ~40 KB).
// Sending one frame takes ~28 ms at 26.7 MHz and loop() waits that long,
// so only ~10 frames per second.
//
// Scene per mood (state.ledStatus, chosen by main.cpp):
//   IDLE        night sky, moon, stars, the flower sleeps ("z z z"), a ladybug climbs the stem
//   CONNECTING  sunset sky with pink clouds and stars, Wi-Fi icon fills up, the flower looks around
//   OKAY        blue sky with white clouds, pollen, a butterfly circles the flower, the eyes follow it
//   NEED_WATER  the brightest scene: blazing orange sky, big sun, heat waves, cracked soil,
//               the flower droops (with a sigh) and is sad
//   WATERING    fresh light sky, a yellow watering can pours, water runs up the stem, splashes
//   LOW_WATER   a big glowing drop (filled up to the tank level), the flower looks at it
//   ERROR       grey storm clouds, "x x" eyes, big warning sign, short glitches, petals fall
// Always: soil colour = soil index, petals fall off when the soil is too dry,
// a heart and a hop after watering, petal colour from the plant name.
//
// Rules: see ANIMATION_LIMITS.md. Only shapes (no per-pixel math), no memory
// allocation per frame, only fixed size static arrays, every index bounded.

#include <Arduino.h>
#include <SPI.h>
#include <esp_system.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include "config.h"
#include "state.h"
#include "logger.h"
#include "display.h"

#define SCREEN_SIZE  240
#define CANVAS_W     120   // flower canvas in RAM, shown x2
#define CANVAS_H     96
#define FLOWER_Y     24    // screen y where the flower area starts
#define DATA_Y       216   // screen y of the data line
#define TEXT_LINE_H  24    // height of the name line and the data line

// Scene geometry, canvas coordinates
#define SOIL_Y         70  // top of the soil in the pot, drops land here
#define POT_TOP_Y      74  // pot rim
#define STEM_BASE_X    60
#define STEM_BASE_Y    78
#define STEM_LENGTH    46
#define SKY_FADE_ROWS  8   // the top and bottom rows melt into the black text lines

// ===================== Tuning =====================
static const float MOOD_EASE = 0.15f;   // lean / petal colour: part of the way to the target per frame
static const float FX_EASE   = 0.10f;   // scene objects slide / fade in and out in ~1 s
static const float SKY_EASE  = 0.06f;   // sky colour changes in ~2 s
static const float SOIL_EASE = 0.05f;
static const float LOOK_EASE = 0.20f;   // eyes turn to what the flower looks at
static const float DROP_GRAVITY   = 0.35f;   // canvas px per frame^2
static const float SPLASH_GRAVITY = 0.35f;
static const float PETAL_GRAVITY  = 0.08f;   // falling petals are light, like paper
static const float MAX_PETAL_LOSS = 0.375f;  // completely dry soil: 3 of 8 petals are gone
static const float GLITCH_PERIOD_S = 3.0f;   // error: one short picture glitch every 3 s
static const uint32_t HEART_MS = 1800;  // heart rising after watering
static const uint32_t HOP_MS   = 900;   // happy hop after watering
static const uint32_t SHAKE_MS = 600;   // head shake when an error starts
static const uint32_t SIGH_MS  = 1600;  // extra droop when the soil turns dry
static const uint32_t BLINK_MS = 150;
static const uint8_t SPLASH_LIFE = 5;    // frames a splash pixel flies
static const uint8_t PETAL_LIFE  = 70;   // frames, long enough to reach the ground
static const float SPIN_SPEED = 0.6f;    // head rotation, rad/s
static const float DROP_RADIUS = 11;     // low water drop: radius of its round part (canvas px)
static const float WARN_HALF   = 10;     // error sign: half width of the inner triangle (canvas px)
static const float WARN_HEIGHT = 20;     //   and its height; the rounded rim adds 5 px all around

static Adafruit_ST7789 tft(-1, DISPLAY_DC_PIN, DISPLAY_RST_PIN);  // -1: the module has no CS pin
static GFXcanvas16* canvas = nullptr;   // created in displayBegin(), nullptr = no RAM
static bool displayOk = false;
static uint16_t lineBuf[SCREEN_SIZE];   // one screen row, for the x2 scaling

// ===================== Colors =====================

struct RGB8 { uint8_t r, g, b; };

// 8-bit R, G, B -> 16-bit display color (RGB565). Values outside 0..255 are clamped.
static uint16_t rgb(int r, int g, int b) {
  r = constrain(r, 0, 255);
  g = constrain(g, 0, 255);
  b = constrain(b, 0, 255);
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

static uint16_t rgbOf(RGB8 c) { return rgb(c.r, c.g, c.b); }

// k = 0 -> a, k = 1 -> b
static RGB8 mix(RGB8 a, RGB8 b, float k) {
  k = constrain(k, 0.0f, 1.0f);
  return { (uint8_t)(a.r + (b.r - a.r) * k), (uint8_t)(a.g + (b.g - a.g) * k),
           (uint8_t)(a.b + (b.b - a.b) * k) };
}

// Hue 0..360, saturation and value 0..1 -> RGB. Easy smooth color shifts.
static RGB8 hsvRGB(float h, float s, float v) {
  h = fmodf(h, 360.0f);
  if (h < 0) h += 360.0f;
  s = constrain(s, 0.0f, 1.0f);
  v = constrain(v, 0.0f, 1.0f);
  float c = v * s;
  float x = c * (1 - fabsf(fmodf(h / 60.0f, 2) - 1));
  float m = v - c;
  float r, g, b;
  if      (h < 60)  { r = c; g = x; b = 0; }
  else if (h < 120) { r = x; g = c; b = 0; }
  else if (h < 180) { r = 0; g = c; b = x; }
  else if (h < 240) { r = 0; g = x; b = c; }
  else if (h < 300) { r = x; g = 0; b = c; }
  else              { r = c; g = 0; b = x; }
  return { (uint8_t)((r + m) * 255), (uint8_t)((g + m) * 255), (uint8_t)((b + m) * 255) };
}

static uint16_t hsv(float h, float s, float v) { return rgbOf(hsvRGB(h, s, v)); }

// ===================== Time =====================
// The clock is a double: a float clock in seconds loses precision after a few
// days (0.06 s steps after 10 days) and the animation would start to stutter.

static double nowSec = 0;      // seconds since boot, set once per frame
static uint32_t nowMs = 0;     // millis() of this frame
static uint32_t frameNo = 0;

// Angle for sinf()/cosf(): sinf(phase(w)) == sin(w * t), always 0..2*PI
static float phase(float speed) { return (float)fmod(nowSec * speed, 2.0 * PI); }

// 0..1 sawtooth that repeats every periodS seconds
static float cycle(float periodS) { return (float)fmod(nowSec / periodS, 1.0); }

static void approach(float& v, float target, float k) { v += (target - v) * k; }

// Soft start and stop for slide-in movements
static float smooth(float a) {
  a = constrain(a, 0.0f, 1.0f);
  return a * a * (3 - 2 * a);
}

// ===================== Mood =====================

enum ParticleKind : uint8_t { PARTICLES_NONE, PARTICLES_POLLEN, PARTICLES_DROPS };

// How the flower and the sky look in one status
struct Mood {
  float lean;          // stem angle, rad: 0 = upright, 0.7 = hanging to the side
  float swayAmp;       // how strongly it sways in the "wind"; 0 = frozen
  float saturation;    // petal color: 1 = bright, 0 = grey
  int8_t mouth;        // +1 smile, 0 flat, -1 sad
  bool crossEyes;      // "x x" eyes (error)
  bool sleepy;         // closed eyes, "z z z" (no sensor data yet)
  ParticleKind particles;
  RGB8 skyTop, skyBottom;
  RGB8 cloud;          // cloud colour
  float clouds;        // 0 = clear sky, 1 = clouds fully visible
};

// Same priority as the LED strip: main.cpp already picked state.ledStatus.
// Skies: friendly and medium bright; NEED_WATER is on purpose the brightest and hottest.
static Mood moodFor(LedStatus s) {
  switch (s) {
    case STATUS_ERROR:      return { 0.15f, 0.00f, 0.00f,  0, true,  false, PARTICLES_NONE,
                                     {30, 30, 40},   {78, 78, 92},    {112, 112, 128}, 0.9f };  // grey storm
    case STATUS_WATERING:   return { 0.00f, 0.15f, 1.00f, +1, false, false, PARTICLES_DROPS,
                                     {70, 160, 215}, {185, 232, 245}, {255, 255, 255}, 1.0f };  // fresh after rain
    case STATUS_LOW_WATER:  return { 0.25f, 0.10f, 0.70f,  0, false, false, PARTICLES_NONE,
                                     {40, 58, 100},  {110, 128, 160}, {190, 200, 222}, 0.6f };  // calm, the drop glows
    case STATUS_NEED_WATER: return { 0.70f, 0.05f, 0.35f, -1, false, false, PARTICLES_NONE,
                                     {255, 90, 10},  {255, 214, 70},  {255, 240, 200}, 0.0f };  // blazing heat
    case STATUS_WIFI_CONNECTING:
                            return { 0.00f, 0.18f, 0.80f, +1, false, false, PARTICLES_NONE,
                                     {36, 28, 92},   {214, 110, 140}, {255, 175, 190}, 0.8f };  // sunset
    case STATUS_IDLE:       return { 0.12f, 0.08f, 0.60f,  0, false, true,  PARTICLES_NONE,
                                     {6, 8, 36},     {38, 40, 100},   {62, 66, 115},   0.4f };  // night
    default:                return { 0.00f, 0.22f, 0.80f, +1, false, false, PARTICLES_POLLEN,
                                     {34, 110, 200}, {140, 205, 240}, {255, 255, 255}, 1.0f };  // okay: sunny day
  }
}

// Everything below moves to its target step by step, nothing jumps
static float curLean = 0;
static float curSat  = 0.8f;
static float sky[6]  = { 34, 110, 200, 140, 205, 240 };   // top r g b, bottom r g b
static float cloudCol[3] = { 255, 255, 255 };
static float cloudAmt = 1;
static RGB8 skyAt(int y);                            // defined in "Background"
static uint16_t overSky(int y, RGB8 c, float alpha);
static float soilWet = 0.5f;                         // 0 = dry (light), 1 = wet (dark)
static float lookX   = 0;                            // eyes: -1 left .. +1 right
static float wind    = 1;                            // 1 = leaves, grass and head move, 0 = frozen (error)
static float headSpin = 0;                           // head rotation, rad; slows down instead of jumping

// Scene objects: 0 = gone, 1 = fully there
struct SceneFx { float stars, sun, fly, wifi, drop, warn, sleep, can, bug; };
static SceneFx fx = {};

static float headX = 60, headY = 32;     // last head position (pollen, z, heart start there)
static float spoutX = -50, spoutY = -50; // tip of the watering can, drops start there

// Mood changes: short one-time reactions (hop, sigh, shake)
static LedStatus shownStatus = STATUS_IDLE;
static LedStatus prevStatus  = STATUS_IDLE;
static uint32_t statusSince  = 0;        // millis() of the last mood change
static bool firstFrame = true;

// ===================== Plant colour (from the plant name) =====================

static float baseHue = 330;     // petal hue; 330 = the classic pink
static const float HUE_SWING = 30;   // the hue drifts back and forth by this much
static const float PETAL_HUES[] = { 330, 285, 12, 200, 300 };
#define COUNT_OF(a) (sizeof(a) / sizeof((a)[0]))

// Same plant name -> always the same colour. No profile -> the classic pink.
static void pickHue(const char* name, bool hasProfile) {
  uint32_t hash = 5381;
  for (const char* p = name; *p; p++) hash = hash * 33 + (uint8_t)*p;
  baseHue = hasProfile ? PETAL_HUES[hash % COUNT_OF(PETAL_HUES)] : 330;
}

static bool hasProfile() { return state.profile.plantId >= 0 && state.profile.name[0]; }
static const char* plantName() { return hasProfile() ? state.profile.name : "BloomBot"; }

// ===================== Particles =====================

// Pollen rises from the head, drops fall from the watering can
struct Particle { float x, y, vx, vy, wobble; ParticleKind kind; bool alive; };
static const int PARTICLE_COUNT = 12;
static Particle parts[PARTICLE_COUNT];
static ParticleKind particleKind = PARTICLES_NONE;

static void spawnPollen(Particle& p) {
  p.x = headX + random(-5, 6);
  p.y = headY + random(-2, 3);
  p.vx = 0;
  p.vy = -(0.5f + random(0, 70) / 100.0f);   // canvas pixels per frame, upwards
  p.wobble = random(0, 628) / 100.0f;
  p.kind = PARTICLES_POLLEN;
  p.alive = true;
}

static void spawnDrop(Particle& p) {
  p.x = spoutX + random(-10, 11) / 10.0f;
  p.y = spoutY;
  p.vx = 0.3f + random(0, 30) / 100.0f;      // the can pours a little to the right
  p.vy = 0.5f + random(0, 50) / 100.0f;
  p.wobble = 0;
  p.kind = PARTICLES_DROPS;
  p.alive = true;
}

// Pollen mood starts: fill the free slots, spread over the whole way up.
// Particles of the old mood are not deleted, they finish their way.
static void startPollen() {
  for (int i = 0; i < PARTICLE_COUNT; i++) {
    if (parts[i].alive) continue;
    spawnPollen(parts[i]);
    parts[i].y = random(0, (long)headY + 1);
  }
}

// Splashes where a drop hits the soil: ring buffer, the oldest is reused
struct Splash { float x, y, vx, vy; uint8_t life; };
static const int SPLASH_COUNT = 10;
static Splash splashes[SPLASH_COUNT];
static uint8_t splashNext = 0;

static void spawnSplash(float x, float y) {
  for (int side = -1; side <= 1; side += 2) {
    Splash& s = splashes[splashNext];
    splashNext = (splashNext + 1) % SPLASH_COUNT;
    s.x = x;
    s.y = y - 1;
    s.vx = side * (0.4f + random(0, 60) / 100.0f);
    s.vy = -(0.9f + random(0, 60) / 100.0f);
    s.life = SPLASH_LIFE;
  }
}

// Petals that fall off (dry soil, error): ring buffer as well
struct FallingPetal { float x, y, vx, vy, wobble; uint16_t color; uint8_t life; };
static const int FALLING_COUNT = 6;
static FallingPetal falling[FALLING_COUNT];
static uint8_t fallingNext = 0;

static void spawnFallingPetal(float x, float y, uint16_t color) {
  FallingPetal& f = falling[fallingNext];
  fallingNext = (fallingNext + 1) % FALLING_COUNT;
  f.x = x;
  f.y = y;
  f.vx = random(-5, 6) / 10.0f;
  f.vy = -0.3f;
  f.wobble = random(0, 628) / 100.0f;
  f.color = color;
  f.life = PETAL_LIFE;
}

// Every particle finishes its way even when the mood changes;
// new ones only start while their mood is shown
static void drawParticles() {
  bool newDrop = particleKind == PARTICLES_DROPS && fx.can > 0.9f;   // one per frame -> a steady stream
  float drift = phase(2.0f);
  for (int i = 0; i < PARTICLE_COUNT; i++) {
    Particle& p = parts[i];
    if (!p.alive) {
      if (newDrop) { spawnDrop(p); newDrop = false; }
      continue;
    }
    if (p.kind == PARTICLES_POLLEN) {
      p.y += p.vy;
      p.x += 0.4f * sinf(drift + p.wobble);
      if (p.y < 0) {
        if (particleKind == PARTICLES_POLLEN) spawnPollen(p);
        else p.alive = false;
        continue;
      }
      float alpha = constrain(p.y / fmaxf(headY, 1.0f), 0.2f, 1.0f);   // fades out on the way up
      canvas->drawPixel(roundf(p.x), roundf(p.y), overSky(p.y, { 255, 225, 60 }, alpha));
    } else {
      p.vy += DROP_GRAVITY;
      p.x += p.vx;
      p.y += p.vy;
      if (p.y >= SOIL_Y) {
        spawnSplash(p.x, SOIL_Y);
        p.alive = false;
        continue;
      }
      canvas->drawFastVLine(roundf(p.x), roundf(p.y), 2, rgb(25, 95, 215));
    }
  }
}

static void drawSplashes() {
  for (int i = 0; i < SPLASH_COUNT; i++) {
    Splash& s = splashes[i];
    if (s.life == 0) continue;
    s.x += s.vx;
    s.y += s.vy;
    s.vy += SPLASH_GRAVITY;
    s.life--;
    canvas->drawPixel(roundf(s.x), roundf(s.y), rgb(60, 140, 235));
  }
}

static void drawFallingPetals() {
  float sway = phase(3.0f);
  for (int i = 0; i < FALLING_COUNT; i++) {
    FallingPetal& f = falling[i];
    if (f.life == 0) continue;
    f.vy = fminf(f.vy + PETAL_GRAVITY, 1.2f);
    f.x += f.vx * 0.3f + 0.6f * sinf(sway + f.wobble);   // see-saw like a leaf
    f.y += f.vy;
    f.life--;
    if (f.y > CANVAS_H + 2) { f.life = 0; continue; }
    canvas->fillCircle(roundf(f.x), roundf(f.y), f.life > 10 ? 2 : 1, f.color);
  }
}

// ===================== Background =====================

// Sky colour of one canvas row: vertical gradient, top and bottom rows fade to black
// so the picture melts into the black name and data lines.
static RGB8 skyAt(int y) {
  y = constrain(y, 0, CANVAS_H - 1);
  float k = y / (float)(CANVAS_H - 1);
  int edge = min(y, CANVAS_H - 1 - y);
  float fade = (edge < SKY_FADE_ROWS) ? (edge + 1) / (float)(SKY_FADE_ROWS + 1) : 1.0f;
  return { (uint8_t)((sky[0] + (sky[3] - sky[0]) * k) * fade),
           (uint8_t)((sky[1] + (sky[4] - sky[1]) * k) * fade),
           (uint8_t)((sky[2] + (sky[5] - sky[2]) * k) * fade) };
}

// "Transparent" colour: c over the sky of row y, alpha 0..1
static uint16_t overSky(int y, RGB8 c, float alpha) { return rgbOf(mix(skyAt(y), c, alpha)); }

static void drawSky() {
  for (int y = 0; y < CANVAS_H; y++) canvas->drawFastHLine(0, y, CANVAS_W, rgbOf(skyAt(y)));
}

// Clouds drift slowly to the right and come back from the left
struct Cloud { float x; int8_t y; uint8_t size; float speed; };
static Cloud clouds[] = { {12, 15, 6, 0.10f}, {78, 25, 5, 0.07f}, {50, 8, 4, 0.13f} };

static void drawClouds() {
  RGB8 col = { (uint8_t)cloudCol[0], (uint8_t)cloudCol[1], (uint8_t)cloudCol[2] };
  for (size_t i = 0; i < COUNT_OF(clouds); i++) {
    Cloud& c = clouds[i];
    c.x += c.speed;
    if (c.x - c.size * 2 > CANVAS_W) c.x = -c.size * 2;
    if (cloudAmt < 0.03f) continue;
    int s = c.size, x = roundf(c.x), y = c.y;
    int rl = s * 2 / 3, rr = s * 3 / 4;                // three puffs with a flat bottom
    uint16_t body = overSky(y, col, cloudAmt);
    canvas->fillCircle(x - s, y + 1, rl, body);
    canvas->fillCircle(x, y - 1, s, body);
    canvas->fillCircle(x + s, y + 1, rr, body);
    canvas->fillRect(x - s, y + 1, 2 * s + 1, rl + 1, body);
    canvas->drawFastHLine(x - s, y + 1 + rl, 2 * s + 1, overSky(y, col, cloudAmt * 0.6f));   // soft shadow
  }
}

// Crescent moon at night: a bright disc, a second disc in sky colour cuts the bite
static void drawMoon() {
  if (fx.sleep < 0.03f) return;
  int cx = 20, cy = roundf(-12 + 30 * smooth(fx.sleep));   // slides down from above
  const RGB8 moon = { 255, 244, 200 };
  canvas->drawCircle(cx, cy, 9, overSky(cy, moon, 0.25f * fx.sleep));   // soft glow
  canvas->fillCircle(cx, cy, 7, overSky(cy, moon, fx.sleep));
  canvas->fillCircle(cx + 4, cy - 3, 6, rgbOf(skyAt(cy - 3)));
}

static const uint8_t STARS[][2] = {
  {8, 12}, {21, 27}, {34, 9}, {47, 20}, {77, 8}, {90, 24},
  {103, 36}, {113, 13}, {14, 45}, {99, 52}, {69, 28}, {40, 38}
};

static void drawStars() {
  if (fx.stars < 0.02f) return;
  float base = phase(1.7f);
  for (size_t i = 0; i < COUNT_OF(STARS); i++) {
    float twinkle = 0.5f + 0.5f * sinf(base + i * 2.1f);
    float v = fx.stars * (0.3f + 0.7f * twinkle);
    int x = STARS[i][0], y = STARS[i][1];
    const RGB8 star = { 255, 245, 200 };
    canvas->drawPixel(x, y, overSky(y, star, v));
    if (twinkle > 0.9f && fx.stars > 0.5f) {   // brightest moment: a small cross
      uint16_t glow = overSky(y, star, v * 0.45f);
      canvas->drawPixel(x - 1, y, glow);
      canvas->drawPixel(x + 1, y, glow);
      canvas->drawPixel(x, y - 1, glow);
      canvas->drawPixel(x, y + 1, glow);
    }
  }
}

// Drought: a big blazing sun with a halo and turning rays
static void drawSun() {
  if (fx.sun < 0.02f) return;
  float cx = 18, cy = roundf(-18 + 34 * smooth(fx.sun));   // slides down from above
  float spin = phase(0.35f);
  float pulse = sinf(phase(2.5f));
  canvas->fillCircle(cx, cy, 12, overSky(cy, { 255, 250, 205 }, 0.45f));   // halo
  uint16_t ray = rgb(255, 250, 190);
  for (int i = 0; i < 12; i++) {
    float a = spin + i * TWO_PI / 12;
    float tip = 17.0f + 2.0f * pulse * ((i & 1) ? 1 : -1);   // long and short rays swap
    canvas->fillTriangle(roundf(cx + cosf(a - 0.12f) * 13), roundf(cy + sinf(a - 0.12f) * 13),
                         roundf(cx + cosf(a + 0.12f) * 13), roundf(cy + sinf(a + 0.12f) * 13),
                         roundf(cx + cosf(a) * tip), roundf(cy + sinf(a) * tip), ray);
  }
  canvas->fillCircle(cx, cy, 9, rgb(255, 222, 50));
  canvas->fillCircle(cx - 2, cy - 2, 5, rgb(255, 248, 170));
}

// Drought: shimmering hot air rises next to the pot
static const int8_t HEAT_WAVES[][3] = { {6, 82, 24}, {88, 88, 24}, {14, 66, 18} };   // x, start y, length

static void drawHeatWaves() {
  if (fx.sun < 0.3f) return;
  float wobble = phase(6.0f);
  const RGB8 hot = { 255, 255, 235 };
  for (size_t i = 0; i < COUNT_OF(HEAT_WAVES); i++) {
    float p = fmodf(cycle(2.8f) + i / 3.0f, 1.0f);
    int y = roundf(HEAT_WAVES[i][1] - p * 26);                  // rises
    float alpha = fx.sun * 0.65f * sinf(p * PI);                // fades in and out
    uint16_t c = overSky(y, hot, alpha);
    for (int dx = 0; dx < HEAT_WAVES[i][2]; dx++) {
      canvas->drawPixel(HEAT_WAVES[i][0] + dx, y + (int)roundf(1.2f * sinf(wobble + dx * 0.5f)), c);
    }
  }
}

// ===================== Flower =====================

// The stem is a quadratic Bezier curve from the pot to the head
struct Stem { float bx, by, cx, cy, tx, ty; };

// Point on the stem, k = 0..1 from base to tip
static void stemAt(const Stem& s, float k, float& x, float& y) {
  float u = 1 - k;
  x = u * u * s.bx + 2 * u * k * s.cx + k * k * s.tx;
  y = u * u * s.by + 2 * u * k * s.cy + k * k * s.ty;
}

static Stem makeStem(const Mood& m, float hop, float sigh) {
  float angle = curLean + sigh + m.swayAmp * sinf(phase(1.3f));
  float length = STEM_LENGTH + hop;
  Stem s;
  s.bx = STEM_BASE_X;
  s.by = STEM_BASE_Y;
  s.tx = s.bx + length * sinf(angle);
  s.ty = s.by - length * cosf(angle);
  s.cx = s.bx;
  s.cy = s.by - 28;   // keeps the stem bottom vertical -> natural bend
  return s;
}

// Watering: a band of water runs up the stem
static void drawStem(const Stem& s) {
  RGB8 green = hsvRGB(120, 0.65f * (0.45f + 0.55f * curSat), 0.67f);
  const RGB8 water = { 120, 200, 255 };
  float front = cycle(1.1f);   // position of the water front, 0..1 from base to tip
  for (int i = 0; i <= 20; i++) {
    float k = i / 20.0f;
    float behind = front - k;
    if (behind < 0) behind += 1;                      // 0 = right at the front
    float wet = fx.can * fmaxf(0.0f, 1.0f - behind * 4.0f);
    float x, y;
    stemAt(s, k, x, y);
    canvas->fillCircle(roundf(x), roundf(y), 1, rgbOf(mix(green, water, wet)));
  }
}

// Leaf made of two triangles; dir = -1 (left) or +1 (right)
static void drawLeaf(float bx, float by, int dir, float flap) {
  float tipX = bx + dir * 14, tipY = by - 5 + flap;
  float midX = bx + dir * 7,  midY = by - 3 + flap * 0.5f;
  float sat = 0.45f + 0.55f * curSat;
  uint16_t leafColor = hsv(120, 0.63f * sat, 0.75f);
  canvas->fillTriangle(bx, by, midX, midY - 3, tipX, tipY, leafColor);
  canvas->fillTriangle(bx, by, midX, midY + 3, tipX, tipY, leafColor);
  canvas->drawLine(bx, by, tipX, tipY, hsv(120, 0.67f * sat, 0.47f));   // vein
}

// Leaves flap in the wind and hang down when the flower droops
static void drawLeaves(const Stem& s) {
  float flap = 1.5f * sinf(phase(1.3f) + 1.0f) * wind + curLean * 6;
  float x, y;
  stemAt(s, 0.35f, x, y);
  drawLeaf(x, y, -1, flap);
  stemAt(s, 0.55f, x, y);
  drawLeaf(x, y, +1, flap);
}

// Climbs up and down the stem; when it leaves it crawls back into the soil
static void drawLadybug(const Stem& s) {
  if (fx.bug < 0.03f) return;
  float c = cycle(26.0f);
  bool up = c < 0.5f;
  float crawl = up ? c * 2 : (1 - c) * 2;
  float k = 0.04f + (0.08f + 0.5f * crawl) * fx.bug;
  float x, y;
  stemAt(s, k, x, y);
  int bx = roundf(x) + 2, by = roundf(y);
  canvas->fillCircle(bx, by, 2, rgb(220, 30, 30));
  canvas->drawFastHLine(bx - 1, up ? by - 2 : by + 2, 3, ST77XX_BLACK);   // head points where it goes
  canvas->drawPixel(bx - 1, by, ST77XX_BLACK);                            // spots
  canvas->drawPixel(bx + 1, up ? by + 1 : by - 1, ST77XX_BLACK);
}

// Soil colour shows the soil index; cracks when it is hot and dry
static void drawPot(bool cracked) {
  const RGB8 dry = { 150, 112, 70 }, wet = { 66, 40, 24 };
  RGB8 soil = mix(dry, wet, soilWet);
  canvas->fillRoundRect(40, SOIL_Y, 40, 6, 2, rgbOf(soil));
  if (cracked) {
    uint16_t crack = rgbOf(mix(soil, { 30, 15, 5 }, 0.6f));
    canvas->drawLine(45, 71, 47, 72, crack);
    canvas->drawLine(53, 72, 55, 71, crack);
    canvas->drawLine(65, 71, 67, 73, crack);
    canvas->drawLine(72, 72, 74, 71, crack);
  }
  uint16_t potColor = rgb(200, 100, 60);
  canvas->fillTriangle(40, 79, 80, 79, 45, 95, potColor);   // body = trapezoid
  canvas->fillTriangle(80, 79, 75, 95, 45, 95, potColor);
  canvas->fillRect(47, 81, 2, 12, rgb(230, 150, 110));      // shine
  canvas->fillRoundRect(37, POT_TOP_Y, 46, 6, 2, rgb(180, 85, 50));  // rim
}

// ===================== Head =====================

struct Head { int x, y; float radius, spin, hue; };

static uint16_t petalMask = 0;     // bit i = petal slot i was hidden in the last frame
static uint16_t nextMask  = 0;
static bool petalsReset = true;    // first frame / soil data appeared: no falling burst

// How many of n petal slots are gone because the soil is too dry
static int hiddenPetals(int n) {
  if (state.moistureIndex < 0) return 0;   // no data: do not guess
  int dryAt = state.profile.moistMin ? state.profile.moistMin : DEFAULT_MOIST_MIN;
  if (state.moistureIndex >= dryAt) return 0;
  float dryness = (dryAt - state.moistureIndex) / (float)dryAt;   // dryAt > 0 here
  return (int)(dryness * MAX_PETAL_LOSS * n + 0.5f);
}

// true = draw petal slot i of n. A slot that has just disappeared falls down.
// (i * 5) % n spreads the gaps around the head (n = 8 -> slots 0, 5, 2 go first).
static bool petalShown(int i, int n, int hidden, float px, float py, uint16_t color) {
  if ((i * 5) % n >= hidden) return true;
  nextMask |= (1u << i);
  if (!petalsReset && !(petalMask & (1u << i))) spawnFallingPetal(px, py, color);
  return false;
}

static void drawDaisy(const Head& h) {
  const int N = 8;
  int hidden = hiddenPetals(N);
  uint16_t outer = hsv(h.hue, 0.8f * curSat, 0.6f + 0.4f * curSat);   // curSat = 0 -> grey
  for (int i = 0; i < N; i++) {
    float a = h.spin + i * TWO_PI / N;
    float px = h.x + cosf(a) * h.radius, py = h.y + sinf(a) * h.radius;
    if (petalShown(i, N, hidden, px, py, outer)) canvas->fillCircle(roundf(px), roundf(py), 5, outer);
  }
  uint16_t inner = hsv(h.hue + 15, 0.45f * curSat, 0.7f + 0.3f * curSat);   // shifted by half a step
  for (int i = 0; i < N; i++) {
    float a = h.spin + (i + 0.5f) * TWO_PI / N;
    canvas->fillCircle(roundf(h.x + cosf(a) * h.radius * 0.6f), roundf(h.y + sinf(a) * h.radius * 0.6f), 4, inner);
  }
  canvas->fillCircle(h.x, h.y, 6, hsv(45, 0.15f + 0.7f * curSat, 0.6f + 0.4f * curSat));
}

// Random blinking, sometimes twice. Signed difference: safe when millis() wraps.
static uint32_t blinkAt = 3000;
static uint8_t blinksLeft = 0;

static bool blinking() {
  int32_t d = (int32_t)(nowMs - blinkAt);
  if (d < 0) return false;
  if (d < (int32_t)BLINK_MS) return true;
  if (blinksLeft) {
    blinksLeft--;
    blinkAt = nowMs + 120;                  // quick second blink
  } else {
    blinksLeft = (random(0, 4) == 0) ? 1 : 0;
    blinkAt = nowMs + random(2500, 6000);
  }
  return false;
}

static void drawFace(int hx, int hy, const Mood& m) {
  uint16_t face = rgb(80, 40, 0);
  int look = (int)roundf(constrain(lookX, -1.0f, 1.0f));
  bool blink = blinking();
  for (int side = -1; side <= 1; side += 2) {
    int ex = hx + side * 2, ey = hy - 2;
    if (m.crossEyes) {
      canvas->drawLine(ex - 1, ey - 1, ex + 1, ey + 1, face);
      canvas->drawLine(ex - 1, ey + 1, ex + 1, ey - 1, face);
    } else if (m.sleepy) {                   // closed eyes "u u"
      canvas->drawPixel(ex - 1, ey, face);
      canvas->drawPixel(ex, ey + 1, face);
      canvas->drawPixel(ex + 1, ey, face);
    } else if (blink) {
      canvas->drawFastHLine(ex - 1, ey, 3, face);
    } else {
      canvas->drawFastVLine(ex + look, ey - 1, 2, face);
    }
  }
  if (m.sleepy) {                            // small calm mouth
    canvas->drawFastHLine(hx - 1, hy + 2, 3, face);
    return;
  }
  // Mouth: 5 pixels, the two corners decide smile / sad / flat
  for (int dx = -2; dx <= 2; dx++) {
    int corner = (dx == -2 || dx == 2) ? 1 : 0;
    int y = hy + 2;                                   // flat
    if (m.mouth > 0)      y = hy + 3 - corner;        // corners up = smile
    else if (m.mouth < 0) y = hy + 2 + corner;        // corners down = sad
    canvas->drawPixel(hx + dx, y, face);
  }
  if (m.mouth > 0 && !m.crossEyes) {         // rosy cheeks when happy
    canvas->drawPixel(hx - 4, hy + 1, rgb(255, 120, 140));
    canvas->drawPixel(hx + 4, hy + 1, rgb(255, 120, 140));
  }
}

static void drawHead(int hx, int hy, const Mood& m) {
  Head h;
  h.x = hx;
  h.y = hy;
  h.spin = headSpin;                                         // slows down to a stop on error
  h.radius = 9 + sinf(phase(m.sleepy ? 1.0f : 2.0f));        // "breathing", slower in sleep
  h.hue = baseHue + HUE_SWING * sinf(phase(0.5f));   // pink <-> violet <-> red
  nextMask = 0;
  drawDaisy(h);
  petalMask = nextMask;
  petalsReset = false;
  drawFace(hx, hy, m);
}

// ===================== Scene objects =====================

// The butterfly circles the head; depth < 0 = behind the flower
static void butterflyPos(float& x, float& y, float& depth) {
  float a = phase(0.55f);
  float away = 1 - smooth(fx.fly);          // flies in from the upper right
  depth = cosf(a);
  x = headX + 24 * sinf(a) + away * 80;
  y = headY - 6 + 8 * sinf(phase(1.1f)) - away * 20;
}

static void drawButterfly(bool front) {
  if (fx.fly < 0.02f) return;
  float x, y, depth;
  butterflyPos(x, y, depth);
  if ((depth >= 0) != front) return;
  static const float SPREAD[4] = { 1.0f, 0.65f, 0.3f, 0.65f };   // wing beat, one step per frame
  float s = SPREAD[frameNo & 3] * (front ? 1.0f : 0.75f);        // smaller when behind
  float v = front ? 1.0f : 0.7f;
  uint16_t upper = hsv(28, 0.9f, v), lower = hsv(48, 0.85f, v), body = rgb(50, 25, 10);
  int bx = roundf(x), by = roundf(y);
  for (int side = -1; side <= 1; side += 2) {
    canvas->fillTriangle(bx, by - 1, bx + side * roundf(6 * s), by - 5, bx + side * roundf(5 * s), by + 1, upper);
    canvas->fillTriangle(bx, by + 1, bx + side * roundf(4 * s), by + 4, bx + side, by + 4, lower);
  }
  canvas->drawFastVLine(bx, by - 2, 5, body);
  canvas->drawPixel(bx - 1, by - 3, body);   // antennae
  canvas->drawPixel(bx + 1, by - 3, body);
}

// Watering can in the upper left corner, tilted to pour. Sets spoutX/spoutY.
static void drawCan() {
  if (fx.can < 0.02f) { spoutX = spoutY = -50; return; }
  float a = smooth(fx.can);
  float cx = 22 - (1 - a) * 45, cy = 16;                       // slides in from the left
  float tilt = a * (0.5f + 0.05f * sinf(phase(3.0f)));         // pours, wobbles a little
  float c = cosf(tilt), s = sinf(tilt);
  // local can coordinates -> canvas
  auto px = [&](float lx, float ly) { return (int16_t)roundf(cx + lx * c - ly * s); };
  auto py = [&](float lx, float ly) { return (int16_t)roundf(cy + lx * s + ly * c); };

  uint16_t metal = rgb(245, 195, 45), dark = rgb(175, 120, 20), shine = rgb(255, 245, 170);
  canvas->fillTriangle(px(-8, -6), py(-8, -6), px(8, -6), py(8, -6), px(8, 6), py(8, 6), metal);
  canvas->fillTriangle(px(-8, -6), py(-8, -6), px(8, 6), py(8, 6), px(-8, 6), py(-8, 6), metal);
  canvas->drawLine(px(-6, -6), py(-6, -6), px(-6, -10), py(-6, -10), dark);    // handle
  canvas->drawLine(px(-6, -10), py(-6, -10), px(4, -10), py(4, -10), dark);
  canvas->drawLine(px(4, -10), py(4, -10), px(6, -6), py(6, -6), dark);
  canvas->drawLine(px(6, 2), py(6, 2), px(17, -6), py(17, -6), metal);         // spout, 2 px thick
  canvas->drawLine(px(6, 3), py(6, 3), px(17, -5), py(17, -5), dark);
  canvas->fillCircle(px(17, -6), py(17, -6), 1, dark);                        // nozzle
  canvas->drawLine(px(-7, -5), py(-7, -5), px(6, -5), py(6, -5), shine);       // highlight
  spoutX = px(18, -5);
  spoutY = py(18, -5) + 1;
}

// Rows fromY..toY of a drop shape: round bottom (radius r around cx, cy) and two
// straight sides that touch the circle and meet in the pointed tip. One line per row.
// The tip must be higher than the circle: cy - tipY > r.
static void fillDropRows(float cx, float cy, float r, float tipY, int fromY, int toY, uint16_t color) {
  float d = cy - tipY;
  if (d <= r) return;
  float slope = r / sqrtf(d * d - r * r);   // half width per row of the straight sides
  float touchY = cy - r * r / d;            // where the sides touch the circle
  int top = max(fromY, (int)ceilf(tipY));
  int bottom = min(toY, (int)floorf(cy + r - 0.5f));   // no single-pixel stub at the bottom
  for (int y = top; y <= bottom; y++) {
    float dy = y - cy;
    float half = (y < touchY) ? (y - tipY) * slope : sqrtf(fmaxf(0.0f, r * r - dy * dy));
    int left = roundf(cx - half), right = roundf(cx + half);   // both sides rounded alike
    canvas->drawFastHLine(left, y, right - left + 1, color);
  }
}

// Big drop on the right: "refill the tank". Glowing amber rim, inside the real tank level.
static void drawDrop() {
  if (fx.drop < 0.02f) return;
  const float R = DROP_RADIUS;
  // Whole pixels: the shape is rounded the same way in every frame and does not shimmer
  float cx = roundf(105 + (1 - smooth(fx.drop)) * 40);  // slides in from the right
  float cy = roundf(30 + 1.5f * sinf(phase(2.2f)));     // floats up and down a little
  float tipY = cy - R * 2.1f;
  float glow = 0.5f + 0.5f * sinf(phase(5.0f));         // pulsing rim
  int level = (state.tankPercent < 0) ? 0 : constrain(state.tankPercent, 0, 100);
  int waterY = roundf((cy + R) - ((cy + R) - tipY) * level / 100.0f);

  fillDropRows(cx, cy, R + 2, tipY - 3, 0, CANVAS_H, rgb(255, 110 + 70 * glow, 20 * glow));  // rim
  fillDropRows(cx, cy, R, tipY, 0, waterY - 1, rgb(22, 28, 44));                            // empty
  fillDropRows(cx, cy, R, tipY, waterY, CANVAS_H, rgb(45, 135, 235));                       // water
  if (level > 0 && level < 100) {                       // little waves on the surface
    float wave = phase(5.0f);
    for (int x = roundf(cx - R); x <= roundf(cx + R); x++) {
      if (sinf(wave + x * 0.9f) > 0.3f && canvas->getPixel(x, waterY) == rgb(45, 135, 235)) {
        canvas->drawPixel(x, waterY, rgb(170, 220, 255));
      }
    }
  }
  canvas->fillCircle(roundf(cx - R * 0.45f), roundf(cy - R * 0.35f), 2, rgb(235, 245, 255));  // shine
  canvas->drawPixel(roundf(cx - R * 0.3f), roundf(cy - R * 0.85f), rgb(235, 245, 255));
}

// Upper quarter of a circle (-135..-45 degrees) as a few short lines
static void drawArc(float cx, float cy, float r, uint16_t color) {
  const float A0 = -2.356f, A1 = -0.785f;
  const int SEGMENTS = 5;
  float x0 = cx + r * cosf(A0), y0 = cy + r * sinf(A0);
  for (int i = 1; i <= SEGMENTS; i++) {
    float a = A0 + (A1 - A0) * i / SEGMENTS;
    float x1 = cx + r * cosf(a), y1 = cy + r * sinf(a);
    canvas->drawLine(roundf(x0), roundf(y0), roundf(x1), roundf(y1), color);
    x0 = x1;
    y0 = y1;
  }
}

static void drawWifi() {
  if (fx.wifi < 0.02f) return;
  float cx = 109, cy = 24 - (1 - smooth(fx.wifi)) * 34;   // slides down from above
  int lit = (int)(cycle(1.6f) * 4);                        // 0..3 arcs light up one after another
  uint16_t on = rgb(190, 225, 255), off = rgb(100, 85, 150);
  canvas->fillCircle(roundf(cx), roundf(cy), 1, on);
  for (int i = 0; i < 3; i++) drawArc(cx, cy, 4 + i * 4, i < lit ? on : off);
}

// Triangle with rounded corners: the core triangle grown by r in every direction
// (core + one strip along every edge + one circle at every corner)
static void fillRoundTriangle(const float px[3], const float py[3], int r, uint16_t color) {
  canvas->fillTriangle(roundf(px[0]), roundf(py[0]), roundf(px[1]), roundf(py[1]),
                       roundf(px[2]), roundf(py[2]), color);
  float mx = (px[0] + px[1] + px[2]) / 3, my = (py[0] + py[1] + py[2]) / 3;
  for (int i = 0; i < 3; i++) {
    int j = (i + 1) % 3;
    float ex = px[j] - px[i], ey = py[j] - py[i];
    float len = sqrtf(ex * ex + ey * ey);
    if (len < 0.01f) continue;
    float nx = ey / len, ny = -ex / len;                        // edge normal ...
    if (nx * ((px[i] + px[j]) / 2 - mx) + ny * ((py[i] + py[j]) / 2 - my) < 0) {
      nx = -nx;                                                 // ... pointing outwards
      ny = -ny;
    }
    int16_t ax = roundf(px[i]), ay = roundf(py[i]), bx = roundf(px[j]), by = roundf(py[j]);
    int16_t cx = roundf(px[j] + nx * r), cy = roundf(py[j] + ny * r);
    int16_t dx = roundf(px[i] + nx * r), dy = roundf(py[i] + ny * r);
    canvas->fillTriangle(ax, ay, bx, by, cx, cy, color);
    canvas->fillTriangle(ax, ay, cx, cy, dx, dy, color);
    canvas->fillCircle(ax, ay, r, color);
  }
}

// Error: big red warning sign on the right, about as large as the low water drop
static void drawWarning() {
  if (fx.warn < 0.02f) return;
  const float cx = 104;
  float top = roundf(10 + 1.5f * sinf(phase(2.2f)) - (1 - smooth(fx.warn)) * 50);   // slides down, floats
  const float px[3] = { cx, cx - WARN_HALF, cx + WARN_HALF };
  const float py[3] = { top, top + WARN_HEIGHT, top + WARN_HEIGHT };
  float glow = 0.5f + 0.5f * sinf(phase(4.0f));
  fillRoundTriangle(px, py, 5, rgb(255, 70 + 90 * glow, 30 + 30 * glow));   // glowing rim
  fillRoundTriangle(px, py, 3, rgb(200, 25, 35));                          // red sign
  uint16_t mark = rgb(255, 240, 225);
  canvas->fillRoundRect(cx - 1, top + 6, 3, 10, 1, mark);                  // "!"
  canvas->fillCircle(cx, top + 18, 1, mark);
}

// "z z z": grow and fade out on the way up
static void drawSleepZ() {
  if (fx.sleep < 0.05f) return;
  for (int i = 0; i < 3; i++) {
    float p = fmodf(cycle(3.6f) + i / 3.0f, 1.0f);
    float v = fx.sleep * (p < 0.15f ? p / 0.15f : (p > 0.7f ? (1 - p) / 0.3f : 1.0f));
    if (v < 0.1f) continue;
    int size = 2 + (int)(p * 3);
    int x = roundf(headX + 8 + p * 12), y = roundf(headY - 10 - p * 22);
    uint16_t c = overSky(y, { 205, 222, 255 }, v);
    canvas->drawFastHLine(x, y, size + 1, c);
    canvas->drawLine(x + size, y, x, y + size, c);
    canvas->drawFastHLine(x, y + size, size + 1, c);
  }
}

static void drawHeart(uint32_t sinceMs) {
  if (sinceMs >= HEART_MS) return;
  float p = sinceMs / (float)HEART_MS;
  int x = roundf(headX + 7 + 3 * sinf(p * 9));
  int y = roundf(headY - 12 - p * 24);
  int r = (p < 0.1f || p > 0.8f) ? 1 : 2;   // pops up, shrinks away
  uint16_t c = rgb(255, 70, 110);
  canvas->fillCircle(x - r, y, r, c);
  canvas->fillCircle(x + r, y, r, c);
  canvas->fillTriangle(x - 2 * r, y + 1, x + 2 * r, y + 1, x, y + 1 + 2 * r, c);
}

// Error: shift a few rows of the finished picture sideways for a moment
static void shiftRows(int y0, int rows, int shift) {
  if (shift == 0 || abs(shift) >= CANVAS_W) return;
  uint16_t* buf = canvas->getBuffer();
  for (int y = max(y0, 0); y < min(y0 + rows, CANVAS_H); y++) {
    uint16_t* row = buf + y * CANVAS_W;
    if (shift > 0) memmove(row + shift, row, (CANVAS_W - shift) * sizeof(uint16_t));
    else           memmove(row, row - shift, (CANVAS_W + shift) * sizeof(uint16_t));
  }
}

static void drawGlitch() {
  if (fx.warn < 0.9f || cycle(GLITCH_PERIOD_S) > 0.07f) return;   // ~0.2 s every 3 s
  uint32_t seed = (uint32_t)(nowSec / GLITCH_PERIOD_S) * 2654435761u;   // new rows each time
  for (int band = 0; band < 2; band++) {
    int y0 = (seed >> (band * 8)) % (CANVAS_H - 4);
    int amount = 2 + ((seed >> (16 + band * 2)) & 3);                    // 2..5 px
    shiftRows(y0, 3, ((seed >> (band * 8 + 4)) & 1) ? amount : -amount);
  }
}

// ===================== Frame =====================

// Remembers mood changes for the one-time reactions
static void trackStatus(LedStatus s) {
  if (firstFrame) {                     // no reactions right after boot
    shownStatus = prevStatus = s;
    statusSince = nowMs - 60000;
    firstFrame = false;
    return;
  }
  if (s == shownStatus) return;
  prevStatus = shownStatus;
  shownStatus = s;
  statusSince = nowMs;
  if (s == STATUS_ERROR) {              // a few petals drop with the shock
    for (int i = 0; i < 3; i++) {
      spawnFallingPetal(headX + random(-6, 7), headY + random(-4, 5), hsv(baseHue, 0.25f, 0.75f));
    }
  }
}

static float target(bool on) { return on ? 1.0f : 0.0f; }

// Moves every eased value one step closer to the current mood
static void updateScene(const Mood& m, LedStatus s) {
  approach(curLean, m.lean, MOOD_EASE);
  approach(curSat, m.saturation, MOOD_EASE);
  approach(wind, target(m.swayAmp > 0), MOOD_EASE);
  headSpin = fmodf(headSpin + wind * SPIN_SPEED * DISPLAY_FRAME_MS / 1000.0f, TWO_PI);
  const RGB8 goal[2] = { m.skyTop, m.skyBottom };
  for (int i = 0; i < 2; i++) {
    approach(sky[i * 3 + 0], goal[i].r, SKY_EASE);
    approach(sky[i * 3 + 1], goal[i].g, SKY_EASE);
    approach(sky[i * 3 + 2], goal[i].b, SKY_EASE);
  }
  approach(cloudCol[0], m.cloud.r, SKY_EASE);
  approach(cloudCol[1], m.cloud.g, SKY_EASE);
  approach(cloudCol[2], m.cloud.b, SKY_EASE);
  approach(cloudAmt, m.clouds, SKY_EASE);
  approach(fx.stars, target(s == STATUS_IDLE || s == STATUS_WIFI_CONNECTING), FX_EASE);
  approach(fx.sun,   target(s == STATUS_NEED_WATER), FX_EASE);
  approach(fx.fly,   target(s == STATUS_OKAY), FX_EASE);
  approach(fx.wifi,  target(s == STATUS_WIFI_CONNECTING), FX_EASE);
  approach(fx.drop,  target(s == STATUS_LOW_WATER), FX_EASE);
  approach(fx.warn,  target(s == STATUS_ERROR), FX_EASE);
  approach(fx.sleep, target(m.sleepy), FX_EASE);
  approach(fx.can,   target(s == STATUS_WATERING), FX_EASE);
  approach(fx.bug,   target(s == STATUS_IDLE), FX_EASE * 0.5f);

  float wet = (state.moistureIndex < 0) ? 0.5f : constrain(state.moistureIndex, 0, 100) / 100.0f;
  approach(soilWet, fmaxf(wet, fx.can), SOIL_EASE);   // pouring makes the soil dark

  float look = 0;
  if (s == STATUS_OKAY && fx.fly > 0.5f) {             // follows the butterfly
    float bx, by, depth;
    butterflyPos(bx, by, depth);
    look = (bx - headX) / 10.0f;
  } else if (s == STATUS_WIFI_CONNECTING) {
    look = 1.2f * sinf(phase(0.9f));                   // looks around
  } else if (s == STATUS_LOW_WATER) {
    look = 1.2f;                                       // at the drop
  } else if (s == STATUS_WATERING) {
    look = -1.2f;                                      // at the watering can
  }
  approach(lookX, constrain(look, -1.2f, 1.2f), LOOK_EASE);
}

static void drawScene(const Mood& m) {
  uint32_t since = nowMs - statusSince;
  bool afterWatering = prevStatus == STATUS_WATERING &&
                       (shownStatus == STATUS_OKAY || shownStatus == STATUS_IDLE);   // happy only if all is fine
  float hop = 0, sigh = 0, shake = 0;
  if (afterWatering && since < HOP_MS) {
    float p = since / (float)HOP_MS;
    hop = 6 * fabsf(sinf(p * TWO_PI)) * (1 - p);       // two little jumps
  }
  if (shownStatus == STATUS_NEED_WATER && since < SIGH_MS) sigh = 0.25f * sinf(PI * since / (float)SIGH_MS);
  if (shownStatus == STATUS_ERROR && since < SHAKE_MS) shake = ((frameNo & 1) ? 3 : -3) * (1 - since / (float)SHAKE_MS);

  drawSky();
  drawStars();
  drawMoon();
  drawSun();
  drawClouds();
  drawHeatWaves();
  Stem stem = makeStem(m, hop, sigh);
  headX = stem.tx;
  headY = stem.ty;
  drawStem(stem);
  drawLeaves(stem);
  drawLadybug(stem);   // before the pot: it can hide in the soil
  drawPot(fx.sun > 0.5f);
  drawSplashes();
  drawButterfly(false);
  drawCan();           // before the drops: it sets the spout position
  drawParticles();
  drawHead(roundf(stem.tx + shake), roundf(stem.ty), m);
  drawButterfly(true);
  drawFallingPetals();
  if (afterWatering) drawHeart(since);
  drawSleepZ();
  drawDrop();
  drawWifi();
  drawWarning();
  drawGlitch();        // last: it moves what is already drawn
}

// Sends the canvas to the screen, every pixel as 2x2
static void pushCanvasScaled() {
  const uint16_t* buf = canvas->getBuffer();
  tft.startWrite();
  tft.setAddrWindow(0, FLOWER_Y, SCREEN_SIZE, CANVAS_H * 2);
  for (int y = 0; y < CANVAS_H; y++) {
    const uint16_t* row = buf + y * CANVAS_W;
    for (int x = 0; x < CANVAS_W; x++) {
      lineBuf[2 * x]     = row[x];   // x2 in width
      lineBuf[2 * x + 1] = row[x];
    }
    tft.writePixels(lineBuf, SCREEN_SIZE);   // the same row twice = x2 in height
    tft.writePixels(lineBuf, SCREEN_SIZE);
  }
  tft.endWrite();
}

// ===================== Text lines =====================

static char shownName[sizeof(state.profile.name) + 1] = "";   // what is on the screen now
static char shownData[24] = "";

// Text centered in a 24 px strip. Size 2 (12 px per char) if it fits, else size 1.
static void drawTextLine(int y, const char* text, uint16_t color) {
  tft.fillRect(0, y, SCREEN_SIZE, TEXT_LINE_H, ST77XX_BLACK);
  int len = strlen(text);
  uint8_t size = (len * 12 <= SCREEN_SIZE) ? 2 : 1;
  int w = len * 6 * size;   // the built-in font is 6x8 pixels per char at size 1
  int h = 8 * size;
  tft.setTextSize(size);
  tft.setTextColor(color);
  tft.setCursor((SCREEN_SIZE - w) / 2, y + (TEXT_LINE_H - h) / 2);
  tft.print(text);
}

// The server sends names as UTF-8 ("Grünlilie": ü = 2 bytes 0xC3 0xBC).
// The built-in font uses the old PC code page 437 (ü = 1 byte 0x81).
// German letters are converted, any other non-ASCII letter becomes '?'.
static void utf8ToScreen(const char* in, char* out, size_t outSize) {
  size_t o = 0;
  for (size_t i = 0; in[i] && o < outSize - 1; i++) {
    uint8_t c = in[i];
    if (c < 0x80) {
      out[o++] = c;                                  // plain ASCII
    } else if (c == 0xC3 && in[i + 1]) {
      uint8_t n = in[++i];
      switch (n) {
        case 0xA4: out[o++] = 0x84; break;           // ä
        case 0xB6: out[o++] = 0x94; break;           // ö
        case 0xBC: out[o++] = 0x81; break;           // ü
        case 0x84: out[o++] = 0x8E; break;           // Ä
        case 0x96: out[o++] = 0x99; break;           // Ö
        case 0x9C: out[o++] = 0x9A; break;           // Ü
        case 0x9F: out[o++] = 0xE1; break;           // ß
        case 0xA9: out[o++] = 0x82; break;           // é
        default:   out[o++] = '?'; break;
      }
    } else if ((c & 0xC0) == 0xC0) {
      out[o++] = '?';                                // first byte of another letter
    }                                                // else: continuation byte, skip
  }
  out[o] = '\0';
}

// Short German error text for the data line, nullptr = no error
static const char* errorText() {
  if (state.tankSensorFail)     return "Tanksensor";
  if (state.moistureSensorFail) return "Bodensensor";
  if (state.errorReason) {
    if (strcmp(state.errorReason, "pump_timeout") == 0) return "Pumpe";
    if (strcmp(state.errorReason, "still_dry") == 0)    return "Erde trocken";
    return "Giessen";
  }
  return nullptr;
}

// Redraws the name and the data line only when the text changed:
// text drawing is slow and would flicker if repeated every frame
static void updateTexts() {
  const char* name = plantName();
  if (strcmp(name, shownName) != 0) {
    strncpy(shownName, name, sizeof(shownName) - 1);
    char screenName[sizeof(shownName)];
    utf8ToScreen(shownName, screenName, sizeof(screenName));
    drawTextLine(0, screenName, rgb(70, 190, 70));
  }

  char data[sizeof(shownData)];
  uint16_t color = ST77XX_WHITE;
  const char* err = errorText();
  if (err) {
    snprintf(data, sizeof(data), "Fehler: %s", err);
    color = rgb(255, 60, 60);
  } else {
    char soil[12], tank[13];   // -1 = no data -> "--"; room for any int
    if (state.moistureIndex >= 0) snprintf(soil, sizeof(soil), "%d", state.moistureIndex);
    else strcpy(soil, "--");
    if (state.tankPercent >= 0) snprintf(tank, sizeof(tank), "%d%%", state.tankPercent);
    else strcpy(tank, "--");
    snprintf(data, sizeof(data), "Erde %s  Tank %s", soil, tank);
  }
  if (strcmp(data, shownData) != 0) {
    strcpy(shownData, data);
    drawTextLine(DATA_Y, shownData, color);
  }
}

// New plant name -> new petal colour
static void updatePlantStyle() {
  static char styleName[sizeof(state.profile.name) + 1] = "\x01";   // never a real name: first frame picks
  const char* name = plantName();
  if (strcmp(name, styleName) == 0) return;
  strncpy(styleName, name, sizeof(styleName) - 1);
  styleName[sizeof(styleName) - 1] = '\0';
  pickHue(styleName, hasProfile());
  petalsReset = true;
}

// Soil data appears (first reading after boot) or is lost (sensor failure):
// the petal count jumps, that is not "petals falling off"
static void watchSoilData() {
  static bool hadData = false;
  bool hasData = state.moistureIndex >= 0;
  if (hasData != hadData) petalsReset = true;
  hadData = hasData;
}

// ===================== Safe mode and statistics =====================

// Marker in RTC memory: it survives a crash reset (not a power cycle).
// It is set while the display code runs. If the ESP32 crashed with the marker set,
// the display code caused the crash: the animation stays off (safe mode), so the
// device does not crash again and again, and Wi-Fi + OTA keep working for a fixed firmware.
static const uint32_t IN_DISPLAY_MAGIC = 0xD15B1A7Eu;
RTC_NOINIT_ATTR static uint32_t inDisplayCode;
static bool safeMode = false;

static bool lastResetWasCrash() {
  switch (esp_reset_reason()) {
    case ESP_RST_PANIC:
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT:
      return true;
    default:
      return false;
  }
}

static uint32_t statLastUs = 0, statMaxUs = 0, statDrawUs = 0, statSendUs = 0;

void displayPrintStats() {
  if (displayOk) {
    Log.printf("DISPLAY: frame %.1f ms (draw %.1f + send %.1f), max %.1f ms since last 'f'\r\n",
               statLastUs / 1000.0f, statDrawUs / 1000.0f, statSendUs / 1000.0f, statMaxUs / 1000.0f);
    statMaxUs = 0;
  } else {
    Log.println(safeMode ? "DISPLAY: safe mode after crashes, animation off (power cycle or new firmware)"
                         : "DISPLAY: off");
  }
  Log.printf("MEMORY: free %u KB, lowest %u KB, largest block %u KB\r\n",
             (unsigned)(ESP.getFreeHeap() / 1024), (unsigned)(ESP.getMinFreeHeap() / 1024),
             (unsigned)(ESP.getMaxAllocHeap() / 1024));
}

// ===================== Screen recovery (restart after supply dips) =====================
//
// When the pump switches on, its inrush current and the relay coil make the 5 V
// supply dip. The ST7789 controller resets itself and stays in sleep mode: the
// backlight (BLK on 3V3) keeps shining, but the picture is black. The ESP32 keeps
// running and cannot read the screen back (no MISO), so the screen is restarted
// blindly at the moments when a dip is likely:
//   - DISPLAY_REINIT_AFTER_PUMP_ON_MS after pumpOn false -> true (inrush is over),
//   - DISPLAY_REINIT_AFTER_PUMP_OFF_MS after pumpOn true -> false,
//   - every DISPLAY_REINIT_INTERVAL_MS.
// Hard restart (pump edges, command 'R'): RES pulse + the init commands of the
// library. Only RES re-syncs the serial interface (there is no CS pin), but the
// screen is black for ~140 ms.
// Soft restart (periodic): the same init commands without RES. SLPOUT / DISPON do
// nothing to a controller that is awake, so nothing blinks; a controller that fell
// back into sleep mode wakes up.
// Both are a non-blocking step machine on millis(): the pauses the controller needs
// never block loop(), so the pump state machine (and its safety timer) keeps running
// on time. Only the last step sends a few command bytes (well under 1 ms); the next
// normal frame then redraws the whole screen (scene + both text lines).
// tft.init() is NOT used here: it blocks ~600 ms (delay() in the library).

enum ReinitStep { REINIT_IDLE, REINIT_RES_LOW, REINIT_RES_WAIT, REINIT_SLPOUT_WAIT };

static ReinitStep reinitStep = REINIT_IDLE;
static uint32_t reinitStepAt = 0;       // millis() when the current step began
static uint32_t reinitStartedAt = 0;    // millis() when the restart began
static uint32_t lastReinitAt = 0;       // for the periodic restart
static uint32_t reinitCount = 0;        // restarts since boot (log only)
static const char* reinitReason = "";
static bool reinitHard = false;        // current restart uses the RES pulse
static bool pumpWasOn = false;          // last seen state.pumpOn, for edge detection
static uint32_t pumpOnAt = 0;           // millis() of the last pumpOn false -> true
static uint32_t pumpOffAt = 0;          // millis() of the last pumpOn true -> false
static bool reinitAfterPumpOnDue = false;
static bool reinitAfterPumpOffDue = false;
static uint32_t lastFrame = 0;          // frame timer of displayUpdate()

static void drawSafeModeText();

// Starts (or restarts from the beginning) a screen restart.
// hard = with RES pulse (screen black ~140 ms), else only the init commands.
static void startReinit(const char* reason, bool hard) {
  uint32_t now = millis();
  if (hard) {
    digitalWrite(DISPLAY_RST_PIN, LOW);   // hardware reset of the controller
    reinitStep = REINIT_RES_LOW;
  } else {
    tft.sendCommand(ST77XX_SLPOUT);       // no-op if the controller is awake
    reinitStep = REINIT_SLPOUT_WAIT;
  }
  reinitStepAt = now;
  reinitStartedAt = now;
  lastReinitAt = now;
  reinitReason = reason;
  reinitHard = hard;
}

// Advances the restart; returns true while it is running (no drawing then)
static bool runReinit(uint32_t now) {
  switch (reinitStep) {
    case REINIT_IDLE:
      return false;

    case REINIT_RES_LOW:
      if (now - reinitStepAt >= DISPLAY_RESET_LOW_MS) {
        digitalWrite(DISPLAY_RST_PIN, HIGH);
        reinitStep = REINIT_RES_WAIT;
        reinitStepAt = now;
      }
      return true;

    case REINIT_RES_WAIT:
      if (now - reinitStepAt >= DISPLAY_RESET_WAIT_MS) {
        tft.sendCommand(ST77XX_SLPOUT);  // out of sleep mode
        reinitStep = REINIT_SLPOUT_WAIT;
        reinitStepAt = now;
      }
      return true;

    case REINIT_SLPOUT_WAIT: {
      if (now - reinitStepAt < DISPLAY_SLPOUT_WAIT_MS) return true;
      uint32_t t0 = micros();
      // same settings as the library's generic_st7789 init list
      const uint8_t colorMode = 0x55;                 // 16-bit color
      tft.sendCommand(ST77XX_COLMOD, &colorMode, 1);
      tft.setRotation(DISPLAY_ROTATION);              // memory access direction (MADCTL)
      tft.sendCommand(ST77XX_INVON);                  // this panel needs inverted colors
      tft.sendCommand(ST77XX_NORON);
      tft.sendCommand(ST77XX_DISPON);
      uint32_t sendUs = micros() - t0;
      // after a dip the screen memory may be lost: redraw everything with the next frame
      shownName[0] = '\0';
      shownData[0] = '\0';
      lastFrame = now - DISPLAY_FRAME_MS;
      if (safeMode && reinitHard) drawSafeModeText();   // RES may have cleared it
      reinitStep = REINIT_IDLE;
      reinitCount++;
      Log.printf("DISPLAY: %s restart (%s), #%lu, took %lu ms, blocking only %lu us\r\n",
                 reinitHard ? "hard" : "soft", reinitReason, (unsigned long)reinitCount,
                 (unsigned long)(now - reinitStartedAt), (unsigned long)sendUs);
      return false;
    }
  }
  return false;
}

// Watches state.pumpOn and the periodic timer, starts restarts when due
static void scheduleReinit(uint32_t now) {
  bool pumpOn = state.pumpOn;
  if (pumpOn && !pumpWasOn) {           // pump switched on: wait for the inrush to pass
    pumpOnAt = now;
    reinitAfterPumpOnDue = true;
    reinitAfterPumpOffDue = false;
  } else if (!pumpOn && pumpWasOn) {    // pump switched off: let a held-back server call run first
    pumpOffAt = now;
    reinitAfterPumpOnDue = false;
    reinitAfterPumpOffDue = true;
  }
  pumpWasOn = pumpOn;

  if (reinitAfterPumpOnDue && now - pumpOnAt >= DISPLAY_REINIT_AFTER_PUMP_ON_MS) {
    reinitAfterPumpOnDue = false;
    startReinit("pump on", true);
  }
  if (reinitAfterPumpOffDue && now - pumpOffAt >= DISPLAY_REINIT_AFTER_PUMP_OFF_MS) {
    reinitAfterPumpOffDue = false;
    startReinit("pump off", true);
  }
  // periodic: never during the inrush window or while a pump restart is pending
  if (reinitStep == REINIT_IDLE && !reinitAfterPumpOnDue && !reinitAfterPumpOffDue &&
      !state.pumpOn && now - lastReinitAt >= DISPLAY_REINIT_INTERVAL_MS) {
    startReinit("periodic", false);
  }
}

// No drawing right after the pump switched on: a dip during an SPI transfer is
// the most likely moment to upset the controller, and it is restarted anyway
static bool pumpInrushWindow(uint32_t now) {
  return state.pumpOn && now - pumpOnAt < DISPLAY_REINIT_AFTER_PUMP_ON_MS;
}

// ===================== Public =====================

void displayReinitNow() {
  if (!displayOk && !safeMode) {
    Log.println("DISPLAY: off, nothing to restart");
    return;
  }
  Log.println("DISPLAY: restart requested");
  startReinit("manual", true);   // a pending pump-on restart still follows
}

void displayBegin() {
  if (!DISPLAY_ENABLED) return;

  bool crashedHere = lastResetWasCrash() && inDisplayCode == IN_DISPLAY_MAGIC;
  inDisplayCode = IN_DISPLAY_MAGIC;         // displayBegin() itself is guarded as well

  tft.init(SCREEN_SIZE, SCREEN_SIZE, SPI_MODE3);   // no CS pin -> SPI mode 3 is required
  tft.setSPISpeed(DISPLAY_SPI_HZ);
  tft.setRotation(DISPLAY_ROTATION);
  tft.setTextWrap(false);
  tft.cp437(true);   // correct code page 437 table for ä, ö, ü (see utf8ToScreen)
  tft.fillScreen(ST77XX_BLACK);

  lastReinitAt = millis();   // tft.init() just did a full reset
  pumpWasOn = state.pumpOn;

  if (crashedHere) {
    safeMode = true;
    Log.println("DISPLAY: the last crash happened in the display code -> safe mode, animation off");
    drawSafeModeText();
    inDisplayCode = 0;
    return;
  }

  canvas = new GFXcanvas16(CANVAS_W, CANVAS_H);
  if (!canvas->getBuffer()) {
    Log.println("DISPLAY: not enough RAM for the canvas, screen stays off");
  } else {
    displayOk = true;
  }
  inDisplayCode = 0;
}

// Safe mode: no animation, only this text (also redrawn after a restart)
static void drawSafeModeText() {
  tft.fillScreen(ST77XX_BLACK);
  drawTextLine(FLOWER_Y + 84, "Display safe mode", rgb(255, 150, 0));
}

void displayUpdate() {
  if (!displayOk && !safeMode) return;

  uint32_t now = millis();
  scheduleReinit(now);
  if (runReinit(now)) return;                       // restart running: no drawing
  if (!displayOk) return;                           // safe mode: no animation
  if (pumpInrushWindow(now)) return;                // pump just switched on: no drawing
  if (now - lastFrame < DISPLAY_FRAME_MS) return;   // non-blocking frame timer
  lastFrame = now;

  uint32_t t0 = micros();
  inDisplayCode = IN_DISPLAY_MAGIC;
  nowMs = now;
  nowSec = now / 1000.0;
  frameNo++;

  updatePlantStyle();
  watchSoilData();
  LedStatus s = state.ledStatus;
  trackStatus(s);
  Mood m = moodFor(s);
  updateScene(m, s);
  if (m.particles != particleKind) {
    particleKind = m.particles;
    if (particleKind == PARTICLES_POLLEN) startPollen();
  }

  drawScene(m);
  uint32_t t1 = micros();
  pushCanvasScaled();
  uint32_t t2 = micros();
  updateTexts();
  uint32_t t3 = micros();
  inDisplayCode = 0;

  statDrawUs = t1 - t0;
  statSendUs = t2 - t1;
  statLastUs = t3 - t0;
  if (statLastUs > statMaxUs) statMaxUs = statLastUs;
}
