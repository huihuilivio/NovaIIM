/**
 * NovaBridge 抽象接口
 *
 * 平台实现:
 *   - Windows: WebView2 postMessage / __novaBridge.onEvent
 *   - Android / iOS: 待定
 */

export type EventCallback<T = unknown> = (data: T) => void

export interface NovaBridge {
  /** 发送 action 到 C++ */
  send(action: string, data?: Record<string, unknown>): void
  /** 监听 C++ 推送事件 */
  on<T = unknown>(event: string, callback: EventCallback<T>): void
  /** 移除事件监听 */
  off(event: string, callback?: EventCallback): void
}
