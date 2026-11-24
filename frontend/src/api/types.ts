import type { JSONContent } from "@tiptap/react";

export type Role = "user" | "admin";

export interface User {
  id: number;
  email: string;
  role: Role;
}

export interface ParamRule {
  type: "int" | "bool" | "color";
  min?: number;
  max?: number;
  default?: number | boolean | string;
}

export interface CapabilityDef {
  cap_id: string;
  display_name: string;
  icon: string | null;
  component_name: string;
  mqtt_action: string | null;
  telemetry_keys: string[] | null;
  param_schema: Record<string, ParamRule> | null;
  is_active: boolean;
}

export type Telemetry = Record<string, number | string | boolean | null>;

export interface DeviceSummary {
  id: string;
  name: string;
  type_id: string;
  type_name: string;
  capabilities: string[];
  online: boolean;
  state: string | null;
  last_seen: string | null;
  latest: Telemetry | null;
}

export interface DeviceDetail {
  id: string;
  name: string;
  type_id: string;
  type_name: string;
  online: boolean;
  state: string | null;
  last_seen: string | null;
  local_ip: string | null;
  fw_version: string | null;
  capabilities: CapabilityDef[];
  latest: Telemetry | null;
  latest_at: string | null;
  config: Record<string, Record<string, unknown>>;
}

export interface Notification {
  id: number;
  device_id: string | null;
  title: string;
  body: string;
  is_read: boolean;
  created_at: string;
}

export interface TipTapDoc {
  type: "doc";
  content?: JSONContent[];
}

export interface Inquiry {
  id: number;
  title: string;
  status: "open" | "answered" | "closed";
  created_at: string;
  user_email?: string;
  content?: TipTapDoc;
  replies?: { id: number; content: TipTapDoc; created_at: string }[];
}
