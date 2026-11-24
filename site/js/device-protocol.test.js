import { describe, expect, it } from 'vitest'
import {
  COMMANDS,
  LED_STATUS,
  MAX_KEYS_PER_REQUEST,
  STATE_REQUEST_KEY,
  parseStateLine,
  sanitizeKeys,
  splitLines,
} from './device-protocol.js'

const STATE_JSON = {
  st: 2, ph: 'PUMPING', soil: 24, tank: 61, need: 1, att: 1, err: '', last: 50, total: 150,
  wifi: 1, sim: { soil: 24, tank: -1, hose: 1 }, min: 30, tgt: 50, max: 70,
}

describe('sanitizeKeys', () => {
  it('accepts every listed command key and the state request', () => {
    for (const { key } of COMMANDS) expect(sanitizeKeys(key)).toBe(key)
    expect(sanitizeKeys(STATE_REQUEST_KEY)).toBe('j')
  })

  it('accepts several keys up to the limit', () => {
    expect(sanitizeKeys('---')).toBe('---')
    expect(sanitizeKeys('-'.repeat(MAX_KEYS_PER_REQUEST))).toHaveLength(MAX_KEYS_PER_REQUEST)
  })

  it('rejects dangerous or unknown keys', () => {
    expect(sanitizeKeys('F')).toBeNull() // factory reset of the plant profile
    expect(sanitizeKeys('T')).toBeNull() // test profile into flash
    expect(sanitizeKeys('wF')).toBeNull()
    expect(sanitizeKeys('\n')).toBeNull()
  })

  it('rejects empty, too long and non-string input', () => {
    expect(sanitizeKeys('')).toBeNull()
    expect(sanitizeKeys('-'.repeat(MAX_KEYS_PER_REQUEST + 1))).toBeNull()
    expect(sanitizeKeys(undefined)).toBeNull()
    expect(sanitizeKeys(42)).toBeNull()
  })

  it('knows the demo-only keys', () => {
    expect(sanitizeKeys('d')).toBe('d')
    expect(sanitizeKeys('x')).toBe('x')
    expect(COMMANDS.filter((c) => c.only === 'demo').map((c) => c.key)).toEqual(['d', 'x'])
  })

  it('has unique keys', () => {
    const keys = COMMANDS.map((c) => c.key)
    expect(new Set(keys).size).toBe(keys.length)
  })
})

describe('parseStateLine', () => {
  it('turns the firmware line into a view model', () => {
    const s = parseStateLine(`STATE ${JSON.stringify(STATE_JSON)}`)
    expect(s).toMatchObject({
      status: 'watering', phase: 'PUMPING', soil: 24, tank: 61, needWater: true, attempts: 1,
      error: '', lastMl: 50, totalMl: 150, wifi: true, simSoil: true, simTank: false, hoseOn: true,
      moistMin: 30, moistTarget: 50, moistMax: 70, firmware: 'main', tour: false,
    })
  })

  it('recognises the display demo firmware', () => {
    const s = parseStateLine(`STATE ${JSON.stringify({ ...STATE_JSON, fw: 'demo', tour: 1 })}`)
    expect(s.firmware).toBe('demo')
    expect(s.tour).toBe(true)
    expect(s.glassMl).toBeNull()
    expect(parseStateLine(`STATE ${JSON.stringify({ ...STATE_JSON, glass: 120, glassMax: 300 })}`))
      .toMatchObject({ glassMl: 120, glassMaxMl: 300 })
  })

  it('maps all LED status numbers like enum LedStatus', () => {
    LED_STATUS.forEach((name, st) => {
      expect(parseStateLine(`STATE ${JSON.stringify({ ...STATE_JSON, st })}`).status).toBe(name)
    })
    expect(parseStateLine(`STATE ${JSON.stringify({ ...STATE_JSON, st: 99 })}`).status).toBe('idle')
  })

  it('fills defaults for optional fields', () => {
    const { need, att, err, last, total, wifi, min, tgt, max, ...minimal } = STATE_JSON
    const s = parseStateLine(`STATE ${JSON.stringify({ ...minimal, sim: { soil: -1, tank: 40, hose: 0 } })}`)
    expect(s).toMatchObject({
      needWater: false, attempts: 0, error: '', lastMl: 0, totalMl: 0, wifi: false,
      simSoil: false, simTank: true, hoseOn: false, moistMin: null, moistTarget: null, moistMax: null,
    })
  })

  it('ignores other log lines and broken JSON', () => {
    expect(parseStateLine('WATER: soil 24/100')).toBeNull()
    expect(parseStateLine('STATE {broken')).toBeNull()
    expect(parseStateLine('STATE null')).toBeNull()
    expect(parseStateLine('STATE {"st":1}')).toBeNull()
    expect(parseStateLine(`STATE ${JSON.stringify({ ...STATE_JSON, sim: null })}`)).toBeNull()
    expect(parseStateLine(null)).toBeNull()
  })
})

describe('splitLines', () => {
  it('splits complete lines and keeps the unfinished rest', () => {
    expect(splitLines('', 'a\r\nb\nc')).toEqual({ lines: ['a', 'b'], rest: 'c' })
    expect(splitLines('c', 'd\r\n')).toEqual({ lines: ['cd'], rest: '' })
  })

  it('drops empty lines', () => {
    expect(splitLines('', '\r\n\r\nx\r\n')).toEqual({ lines: ['x'], rest: '' })
  })
})
