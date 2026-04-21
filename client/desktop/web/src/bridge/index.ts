/**
 * NovaBridge — 平台通信桥
 *
 * Windows: WebView2 chrome.webview.postMessage ↔ window.__novaBridge.onEvent
 * 其他平台: 待实现
 */

import type { NovaBridge, EventCallback } from './types'

// ---- 全局类型声明 (WebView2) ----

declare global {
  interface Window {
    chrome?: {
      webview?: {
        postMessage(message: string): void
      }
    }
    __novaBridge?: {
      onEvent(msg: { event: string; data: unknown }): void
    }
  }
}

// ---- Windows WebView2 实现 ----

class Win32Bridge implements NovaBridge {
  private listeners = new Map<string, EventCallback[]>()

  constructor() {
    window.__novaBridge = {
      onEvent: (msg) => {
        const cbs = this.listeners.get(msg.event)
        if (cbs) {
          for (const cb of cbs) {
            try {
              cb(msg.data)
            } catch (e) {
              console.error('[Bridge]', e)
            }
          }
        }
      },
    }
  }

  send(action: string, data?: Record<string, unknown>): void {
    window.chrome!.webview!.postMessage(JSON.stringify({ action, ...data }))
  }

  on<T = unknown>(event: string, callback: EventCallback<T>): void {
    const cbs = this.listeners.get(event)
    if (cbs) cbs.push(callback as EventCallback)
    else this.listeners.set(event, [callback as EventCallback])
  }

  off(event: string, callback?: EventCallback): void {
    if (!callback) {
      this.listeners.delete(event)
      return
    }
    const cbs = this.listeners.get(event)
    if (cbs) this.listeners.set(event, cbs.filter((cb) => cb !== callback))
  }
}

// ---- Mock（开发调试用） ----

class MockBridge implements NovaBridge {
  send(action: string, data?: Record<string, unknown>): void {
    console.log('[MockBridge] send:', action, data)
  }
  on(): void {}
  off(): void {}
}

// ---- 工厂 ----

function createBridge(): NovaBridge {
  if (window.chrome?.webview) {
    return new Win32Bridge()
  }
  // TODO: Android / iOS bridge
  console.warn('[Bridge] No native bridge detected, using mock')
  return new MockBridge()
}

export const bridge: NovaBridge = createBridge()
export type { NovaBridge, EventCallback }
