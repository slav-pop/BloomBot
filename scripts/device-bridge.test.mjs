// @vitest-environment node
// Integrationstest der Brücke: ein falsches "ESP32" (TCP-Server) statt bloombot.local:23.
import { mkdtempSync, writeFileSync } from 'node:fs'
import http from 'node:http'
import net from 'node:net'
import { tmpdir } from 'node:os'
import { join } from 'node:path'
import { afterEach, beforeEach, describe, expect, it } from 'vitest'
import { startBridge } from './device-bridge.mjs'

const STATE_LINE = 'STATE {"st":5,"ph":"IDLE","soil":42,"tank":68,"need":0,"att":0,"err":"","last":0,"total":0,"wifi":1,"sim":{"soil":-1,"tank":-1,"hose":1},"min":30,"tgt":50,"max":70}'

function waitFor(check, timeoutMs = 2000) {
  return new Promise((resolve, reject) => {
    const started = Date.now()
    const tick = () => {
      const value = check()
      if (value) return resolve(value)
      if (Date.now() - started > timeoutMs) return reject(new Error('timeout'))
      setTimeout(tick, 10)
    }
    tick()
  })
}

function request(port, { method = 'GET', path = '/', headers = {}, body } = {}) {
  return new Promise((resolve, reject) => {
    const req = http.request({ host: '127.0.0.1', port, method, path, headers }, (res) => {
      let data = ''
      res.on('data', (c) => { data += c })
      res.on('end', () => resolve({ status: res.statusCode, headers: res.headers, body: data }))
    })
    req.on('error', reject)
    if (body !== undefined) req.write(body)
    req.end()
  })
}

function postKeys(port, keys, headers = {}) {
  return request(port, {
    method: 'POST',
    path: '/api/cmd',
    headers: { 'Content-Type': 'application/json', ...headers },
    body: JSON.stringify({ keys }),
  })
}

let device
let deviceSocket
let received
let bridge
let siteDir

beforeEach(async () => {
  received = ''
  deviceSocket = null
  device = net.createServer((socket) => {
    deviceSocket = socket
    socket.on('data', (d) => { received += d.toString() })
  })
  await new Promise((r) => device.listen(0, '127.0.0.1', r))
  siteDir = mkdtempSync(join(tmpdir(), 'bridge-site-'))
  writeFileSync(join(siteDir, 'index.html'), '<h1>BloomBot</h1>')
  bridge = await startBridge({
    siteDir, port: 0, telnetPort: 0, deviceHost: '127.0.0.1', devicePort: device.address().port,
    pollMs: 30, reconnectMs: 30, log: () => {},
  })
  await waitFor(() => deviceSocket)
})

afterEach(async () => {
  await bridge.close()
  deviceSocket?.destroy()
  await new Promise((r) => device.close(r))
})

describe('static site', () => {
  it('serves index.html for /', async () => {
    const res = await request(bridge.port)
    expect(res.status).toBe(200)
    expect(res.headers['content-type']).toContain('text/html')
    expect(res.body).toContain('BloomBot')
  })

  it('does not leave the site folder', async () => {
    expect((await request(bridge.port, { path: '/%2e%2e/%2e%2e/etc/passwd' })).status).toBe(404)
    expect((await request(bridge.port, { path: '/missing.js' })).status).toBe(404)
  })

  it('rejects foreign Host headers (DNS rebinding)', async () => {
    expect((await request(bridge.port, { headers: { Host: 'evil.example' } })).status).toBe(403)
  })
})

describe('commands', () => {
  it('forwards allowed keys to the device', async () => {
    const res = await postKeys(bridge.port, 'D')
    expect(res.status).toBe(200)
    await waitFor(() => received.includes('D'))
  })

  it('rejects keys that are not allowed', async () => {
    expect((await postKeys(bridge.port, 'F')).status).toBe(400)
    expect(received).not.toContain('F')
  })

  it('rejects requests from other websites and without JSON', async () => {
    expect((await postKeys(bridge.port, 'w', { Origin: 'https://evil.example' })).status).toBe(403)
    const plain = await request(bridge.port, { method: 'POST', path: '/api/cmd', body: '{"keys":"w"}' })
    expect(plain.status).toBe(415)
    const broken = await request(bridge.port, {
      method: 'POST', path: '/api/cmd', headers: { 'Content-Type': 'application/json' }, body: '{oops',
    })
    expect(broken.status).toBe(400)
    expect(received).not.toContain('w')
  })

  it('answers 503 while the device is not connected', async () => {
    deviceSocket.destroy()
    await waitFor(() => !bridge.isConnected())
    await new Promise((r) => device.close(r)) // no reconnect possible now
    expect((await postKeys(bridge.port, 'w')).status).toBe(503)
    await new Promise((r) => device.listen(0, '127.0.0.1', r)) // afterEach closes it again
  })
})

describe('device state', () => {
  it('polls the state line and reports it', async () => {
    await waitFor(() => received.includes('j'))
    deviceSocket.write(`LOG: hello\r\n${STATE_LINE}\r\n`)
    await waitFor(() => bridge.lastState())
    const res = await request(bridge.port, { path: '/api/status' })
    expect(JSON.parse(res.body)).toMatchObject({ connected: true, state: { status: 'okay', soil: 42, tank: 68 } })
  })

  it('streams log lines and state as server-sent events', async () => {
    let text = ''
    const req = http.get({ host: '127.0.0.1', port: bridge.port, path: '/api/events' }, (res) => {
      expect(res.headers['content-type']).toContain('text/event-stream')
      res.on('data', (c) => { text += c })
    })
    await waitFor(() => text.includes('"type":"conn"'))
    deviceSocket.write(`WATER: soil 20/100\r\n${STATE_LINE}\r\n`)
    await waitFor(() => text.includes('WATER: soil 20/100') && text.includes('"type":"state"'))
    req.destroy()
  })

  it('shares the device with a local terminal (VS Code Monitor)', async () => {
    let terminalText = ''
    const terminal = net.createConnection({ host: '127.0.0.1', port: bridge.telnetPort })
    terminal.on('data', (d) => { terminalText += d.toString() })
    await waitFor(() => terminalText.includes('BRIDGE'))
    deviceSocket.write(`WATER: soil 20/100\r\n${STATE_LINE}\r\n`)
    await waitFor(() => terminalText.includes('WATER: soil 20/100'))
    expect(terminalText).not.toContain('STATE {') // the page polls every second, the terminal stays readable
    terminal.write('s')
    await waitFor(() => received.includes('s'))
    terminal.destroy()
  })

  it('drops a silent connection (ESP32 left the Wi-Fi) and connects again', async () => {
    await bridge.close()
    const first = deviceSocket
    bridge = await startBridge({
      siteDir, port: 0, deviceHost: '127.0.0.1', devicePort: device.address().port,
      pollMs: 1000, reconnectMs: 20, idleMs: 120, log: () => {},
    })
    await waitFor(() => deviceSocket !== first)
    const silent = deviceSocket // never answers
    await waitFor(() => deviceSocket !== silent, 3000)
  })

  it('reconnects after the device dropped the connection', async () => {
    const first = deviceSocket
    first.destroy()
    await waitFor(() => deviceSocket !== first)
    await waitFor(() => bridge.isConnected())
  })
})
