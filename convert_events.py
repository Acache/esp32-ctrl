#!/usr/bin/env python3
"""
convert_events.py - 把 OpenClaw export-trajectory 出的 events.jsonl 转成 chat_log.md

用法：
    python3 convert_events.py [events.jsonl 的路径]
    # 默认读 .openclaw/trajectory-exports/voice_assistant_design/events.jsonl
"""

import json
import sys
import re
from pathlib import Path
from datetime import datetime

# === 配置 ===
SCRIPT_DIR = Path(__file__).parent
DEFAULT_INPUT = SCRIPT_DIR / ".openclaw" / "trajectory-exports" / "voice_assistant_design" / "events.jsonl"
OUTPUT_FILE = SCRIPT_DIR / "chat_log.md"

# 项目相关关键词
PROJECT_KEYWORDS = [
    "jev 模型的思路", "语音指令", "语音控制", "ESP32",
    "B 站", "Bilibili", "MASTER_SPEC", "我们讨论的最终",
    "继续这个项目", "微雪", "amoled", "AMOLED",
    "SenseVoice", "Qwen", "FastAPI", "MicroPython",
    "openclaw-staged",  # 语音消息标志
]

# 项目讨论起始时间（之前的内容是别的项目）
PROJECT_START_DATE = "2026-10-15T00:00:00Z"  # 包含 10-15/16/17 整个讨论周期


def is_project_related(text: str) -> bool:
    """判断消息是否与项目相关"""
    text_lower = text.lower()
    for kw in PROJECT_KEYWORDS:
        if kw.lower() in text_lower:
            return True
    return False


def extract_text_from_content(content) -> str:
    """从 message content 提取纯文本"""
    if isinstance(content, str):
        return content
    if isinstance(content, list):
        parts = []
        for item in content:
            if isinstance(item, dict):
                if item.get("type") == "text":
                    parts.append(item.get("text", ""))
                elif item.get("type") == "thinking":
                    parts.append("[内部思考已省略]")
                elif item.get("type") == "toolCall":
                    name = item.get("name", "tool")
                    args = item.get("arguments", {})
                    args_str = json.dumps(args, ensure_ascii=False)[:1500]
                    parts.append(f"[工具调用: {name}]\n```json\n{args_str}\n```")
                elif item.get("type") == "toolResult":
                    result = item.get("content", "") or item.get("result", "")
                    parts.append(f"[工具结果]: {str(result)[:800]}")
        return "\n".join(parts)
    return str(content)


def parse_events(events_path: Path):
    """读 events.jsonl，返回 (user_msgs, assistant_msgs) 列表"""
    user_msgs = []
    assistant_msgs = []
    
    with open(events_path, 'r', encoding='utf-8') as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            try:
                ev = json.loads(line)
            except json.JSONDecodeError:
                continue
            
            ev_type = ev.get("type", "")
            ts = ev.get("ts", "")
            data = ev.get("data", {})
            msg = data.get("message", {})
            
            if ev_type == "user.message":
                content = msg.get("content", "")
                if isinstance(content, str):
                    user_msgs.append({"ts": ts, "content": content, "id": msg.get("idempotencyKey", "")})
            elif ev_type == "assistant.message":
                content = msg.get("content", "")
                if isinstance(content, (str, list)):
                    assistant_msgs.append({"ts": ts, "content": content, "id": ev.get("entryId", "")})
    
    return user_msgs, assistant_msgs


def filter_project_messages(user_msgs, assistant_msgs):
    """过滤项目相关消息
    规则：user 消息必须在项目时间范围内 + 包含项目关键词
    assistant 消息全部保留（围绕 project 讨论的）"""
    # 合并按时间排序
    all_msgs = []
    for m in user_msgs:
        m["role"] = "user"
        all_msgs.append(m)
    for m in assistant_msgs:
        m["role"] = "assistant"
        all_msgs.append(m)
    
    all_msgs.sort(key=lambda m: m["ts"])
    
    # 第一步：找 user 消息中明确说"换主题"或"开始 X"的位置
    # 项目从 2026-10-15 左右开始
    project_start_idx = 0
    for i, m in enumerate(all_msgs):
        if m["role"] == "user" and m["ts"] >= "2026-10-15":
            project_start_idx = i
            break
    
    # 第二步：从 project_start_idx 开始，user 消息必须匹配关键词
    project_msgs = []
    for m in all_msgs[project_start_idx:]:
        if m["role"] == "assistant":
            project_msgs.append(m)
        else:  # user
            content_str = m["content"] if isinstance(m["content"], str) else extract_text_from_content(m["content"])
            if is_project_related(content_str):
                project_msgs.append(m)
    
    return project_msgs


def render_markdown(project_msgs) -> str:
    """渲染 markdown"""
    lines = [
        "# 语音控制 Mac 助手 — 对话记录",
        "",
        f"> 生成时间：{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}",
        f"> 来源：OpenClaw export-trajectory (events.jsonl)",
        f"> 项目消息数：{len(project_msgs)}",
        "",
        "> 本文件包含 2026-10-15 起所有与「语音控制 Mac 助手」项目相关的对话。",
        "> 包括用户原始消息、助手思考块（已省略）、工具调用（保留为 JSON 摘要）。",
        "",
        "---",
        "",
    ]
    
    current_date = None
    for m in project_msgs:
        ts = m["ts"]
        date_str = ts.split("T")[0] if "T" in ts else ts
        
        if date_str != current_date:
            current_date = date_str
            lines.append(f"\n## {date_str}\n")
        
        role = m["role"]
        role_icon = "🧑 用户" if role == "user" else "🤖 Assistant"
        lines.append(f"### {ts} · {role_icon}")
        if m.get("id"):
            lines.append(f"`(id: {m['id']})`")
        lines.append("")
        
        content = m["content"]
        if isinstance(content, str):
            lines.append(content)
        else:
            lines.append(extract_text_from_content(content))
        
        lines.append("")
        lines.append("---")
        lines.append("")
    
    return "\n".join(lines)


def main():
    events_path = Path(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_INPUT
    
    if not events_path.exists():
        print(f"文件不存在: {events_path}", file=sys.stderr)
        print(f"提示：先用 'openclaw sessions export-trajectory' 生成数据", file=sys.stderr)
        sys.exit(1)
    
    print(f"读 {events_path}...", file=sys.stderr)
    user_msgs, assistant_msgs = parse_events(events_path)
    print(f"解析出 user: {len(user_msgs)}, assistant: {len(assistant_msgs)}", file=sys.stderr)
    
    project_msgs = filter_project_messages(user_msgs, assistant_msgs)
    print(f"项目相关消息: {len(project_msgs)}", file=sys.stderr)
    
    md = render_markdown(project_msgs)
    OUTPUT_FILE.write_text(md, encoding="utf-8")
    print(f"✅ 已写入 {OUTPUT_FILE}（{len(md)} 字符）", file=sys.stderr)


if __name__ == "__main__":
    main()
