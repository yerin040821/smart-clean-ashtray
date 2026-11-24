import Placeholder from "@tiptap/extension-placeholder";
import { EditorContent, useEditor } from "@tiptap/react";
import StarterKit from "@tiptap/starter-kit";
import type { TipTapDoc } from "../api/types";
import { cx } from "./ui";

/** TipTap 3 — 문서는 JSON으로 저장/전송 (HTML 저장 X → XSS 방지). readOnly로 렌더링 겸용. */
export function RichEditor({ value, onChange, readOnly = false, placeholder }: {
  value?: TipTapDoc; onChange?: (doc: TipTapDoc) => void; readOnly?: boolean; placeholder?: string;
}) {
  const editor = useEditor({
    extensions: [StarterKit, Placeholder.configure({ placeholder: placeholder ?? "내용을 입력하세요…" })],
    content: value ?? "",
    editable: !readOnly,
    onUpdate: ({ editor }) => onChange?.(editor.getJSON() as TipTapDoc),
    editorProps: { attributes: { class: "prose-sm min-h-[8rem] outline-none" } },
  });

  if (!editor) return null;

  const btn = (label: string, active: boolean, run: () => void) => (
    <button
      type="button"
      onClick={run}
      className={cx("rounded px-2 py-1 text-sm", active ? "bg-slate-800 text-white" : "hover:bg-slate-100")}
    >
      {label}
    </button>
  );

  return (
    <div className={cx("rounded-lg", !readOnly && "border border-slate-300")}>
      {!readOnly && (
        <div className="flex flex-wrap gap-1 border-b border-slate-200 p-1">
          {btn("B", editor.isActive("bold"), () => editor.chain().focus().toggleBold().run())}
          {btn("I", editor.isActive("italic"), () => editor.chain().focus().toggleItalic().run())}
          {btn("H2", editor.isActive("heading", { level: 2 }), () => editor.chain().focus().toggleHeading({ level: 2 }).run())}
          {btn("• 목록", editor.isActive("bulletList"), () => editor.chain().focus().toggleBulletList().run())}
          {btn("1. 목록", editor.isActive("orderedList"), () => editor.chain().focus().toggleOrderedList().run())}
          {btn("인용", editor.isActive("blockquote"), () => editor.chain().focus().toggleBlockquote().run())}
          {btn("코드", editor.isActive("codeBlock"), () => editor.chain().focus().toggleCodeBlock().run())}
        </div>
      )}
      <EditorContent editor={editor} className={cx("tiptap", !readOnly && "p-3")} />
    </div>
  );
}
