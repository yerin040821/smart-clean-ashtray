import { useEffect, useRef, useState } from "react";
import { wsUrl } from "../api/client";
import type { Telemetry } from "../api/types";

export interface WsMessage {
  type: "telemetry" | "event" | "status";
  device_id: string;
  ts: string;
  data: Telemetry;
}

/** /ws/devices/{id} 구독 — 끊기면 지수 백오프로 재연결, 25초마다 ping. */
export function useDeviceSocket(deviceId: string | undefined, onMessage: (m: WsMessage) => void) {
  const [connected, setConnected] = useState(false);
  const cb = useRef(onMessage);
  cb.current = onMessage;

  useEffect(() => {
    if (!deviceId) return;
    let ws: WebSocket | null = null;
    let closed = false;
    let retry = 0;
    let ping: ReturnType<typeof setInterval>;
    let timer: ReturnType<typeof setTimeout>;

    const connect = () => {
      ws = new WebSocket(wsUrl(`/ws/devices/${encodeURIComponent(deviceId)}`));
      ws.onopen = () => {
        retry = 0;
        setConnected(true);
        ping = setInterval(() => ws?.readyState === WebSocket.OPEN && ws.send("ping"), 25_000);
      };
      ws.onmessage = (e) => {
        if (e.data === "pong") return;
        try {
          cb.current(JSON.parse(e.data));
        } catch {
          /* ignore */
        }
      };
      ws.onclose = (e) => {
        setConnected(false);
        clearInterval(ping);
        if (closed || e.code === 4401 || e.code === 4403) return;
        timer = setTimeout(connect, Math.min(30_000, 1000 * 2 ** retry++));
      };
    };
    connect();
    return () => {
      closed = true;
      clearInterval(ping);
      clearTimeout(timer);
      ws?.close();
    };
  }, [deviceId]);

  return connected;
}
