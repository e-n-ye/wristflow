"""Loopback operator console shared by rehearsal and simulated collection."""
from __future__ import annotations

import http.server
import json
import secrets
import threading
import time


class Cancelled(Exception):
    pass


PAGE = """<!doctype html><html lang="zh-CN"><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>WristFlow 测试协作</title><style>
body{font:16px system-ui;margin:0;background:#101820;color:#eef4f6}
main{max-width:760px;margin:auto;padding:28px}h1{font-size:26px}
.card{background:#1e2b36;border:1px solid #435663;border-radius:14px;padding:20px;margin:18px 0}
li{padding:8px 0}button{padding:12px 24px;border:0;border-radius:8px;font:inherit;margin:8px 12px 0 0;cursor:pointer}
#confirm{background:#7bdfbf}#cancel{background:#f3b6a5}button:disabled{opacity:.4;cursor:default}
#message{white-space:pre-wrap;line-height:1.7}small{color:#b9cbd6}strong{color:#7bdfbf}
</style><main><h1>WristFlow 测试协作</h1><small id="mode"></small>
<section class="card"><strong id="summary">正在连接</strong><ol id="phases"></ol></section>
<section class="card"><div id="message"></div><small id="remaining"></small><div>
<button id="confirm" disabled>确认并继续</button><button id="cancel">取消本轮</button></div></section>
<small id="error"></small></main><script>
const token=__TOKEN__;let current=null,terminal=false;
const labels={pending:'未执行',running:'进行中',waiting:'等待确认',complete:'已完成',cleaned:'中止后已清理',cancelled:'已取消',timeout:'已超时',error:'异常'};
async function post(path,body){const r=await fetch(path,{method:'POST',headers:{'Content-Type':'application/json','X-Workflow-Token':token},body:JSON.stringify(body)});if(!r.ok)throw Error(await r.text());}
async function update(){try{const r=await fetch('/state',{cache:'no-store'});if(!r.ok)throw Error('状态读取失败');const s=await r.json();current=s.prompt;terminal=!!s.result;
document.querySelector('#mode').textContent=s.mode+' · 本页只记录确认，确认点击不证明物理动作。';
const checks=s.result&&s.result.verification;
document.querySelector('#summary').textContent=checks?(checks.every(c=>c.passed)?'本轮检查通过':'本轮检查未完成')+' · 已验证 '+checks.filter(c=>c.passed).length+' / '+checks.length+' 项 · '+s.result.label:(s.result?s.result.label:'本轮进行中')+' · 已完成 '+s.completed+' / '+s.phases.length+' 个步骤';
const list=document.querySelector('#phases');list.replaceChildren();for(const p of checks||s.phases){const e=document.createElement('li');e.textContent=p.title+'：'+(checks?(p.passed?'通过':'未完成'):labels[p.state]);list.append(e);}
document.querySelector('#message').textContent=current?current.message:(s.result?s.result.detail:'程序正在处理，请稍候。');
document.querySelector('#remaining').textContent=current?'确认剩余 '+Math.max(0,Math.ceil(current.remaining))+' 秒':'';
document.querySelector('#confirm').disabled=terminal||!current;document.querySelector('#cancel').disabled=terminal;
if(terminal){await post('/seen',{});return;}setTimeout(update,250);
}catch(e){document.querySelector('#error').textContent=e.message+'；请用 status 命令读取保存的进度。';}}
document.querySelector('#confirm').onclick=async()=>{if(!current)return;document.querySelector('#confirm').disabled=true;try{await post('/reply',{id:current.id,answer:'continue'});}catch(e){document.querySelector('#error').textContent=e.message;}};
document.querySelector('#cancel').onclick=async()=>{try{await post('/cancel',{});}catch(e){document.querySelector('#error').textContent=e.message;}};update();
</script></html>"""


class Console:
    def __init__(self, journal, phases, port=0, mode="离线演练"):
        self.journal = journal
        self.mode = mode
        self.phases = [{"id": key, "title": title, "state": "pending"} for key, title in phases]
        self.condition = threading.Condition()
        self.cancelled = threading.Event()
        self.final_seen = threading.Event()
        self.prompt_value = None
        self.answer = None
        self.result = None
        self.sealed = False
        self.final_outcome = None
        self.visited = False
        token = secrets.token_urlsafe(32)
        owner = self

        class Handler(http.server.BaseHTTPRequestHandler):
            def log_message(self, *args):
                pass

            def setup(self):
                super().setup()
                self.connection.settimeout(2)

            def send(self, status, value, mime="application/json"):
                body = value.encode("utf-8") if isinstance(value, str) else json.dumps(value, ensure_ascii=False).encode("utf-8")
                self.send_response(status)
                self.send_header("Content-Type", mime + "; charset=utf-8")
                self.send_header("Content-Length", str(len(body)))
                self.send_header("Cache-Control", "no-store")
                self.send_header("X-Content-Type-Options", "nosniff")
                self.end_headers()
                self.wfile.write(body)

            def do_GET(self):
                if self.headers.get("Host") != owner.address:
                    return self.send(403, {"error": "Host rejected"})
                if self.path == "/":
                    owner.visited = True
                    return self.send(200, PAGE.replace("__TOKEN__", json.dumps(token)), "text/html")
                if self.path == "/state":
                    return self.send(200, owner.snapshot())
                self.send(404, {"error": "Unknown path"})

            def do_POST(self):
                if (self.headers.get("Host") != owner.address or
                        self.headers.get("Origin") != owner.url or
                        self.headers.get("X-Workflow-Token") != token):
                    return self.send(403, {"error": "Origin/token rejected"})
                try:
                    length = int(self.headers.get("Content-Length", "0"))
                    if not 0 < length <= 1024:
                        raise ValueError("Invalid request length")
                    value = json.loads(self.rfile.read(length))
                    if self.path == "/reply":
                        owner.respond(value["id"], value["answer"])
                    elif self.path == "/cancel":
                        owner.cancel()
                    elif self.path == "/seen":
                        owner.final_seen.set()
                    else:
                        return self.send(404, {"error": "Unknown path"})
                    self.send(200, {"ok": True})
                except (ValueError, KeyError, TypeError) as exc:
                    self.send(409, {"error": str(exc)})

        self.server = http.server.ThreadingHTTPServer(("127.0.0.1", port), Handler)
        self.address = f"127.0.0.1:{self.server.server_port}"
        self.url = "http://" + self.address
        self.thread = threading.Thread(target=self.server.serve_forever, name="battery-console", daemon=True)

    def __enter__(self):
        self.thread.start()
        return self

    def __exit__(self, *args):
        if self.visited and self.result:
            self.final_seen.wait(1)
        self.server.shutdown()
        self.server.server_close()
        self.thread.join(2)
        if self.thread.is_alive():
            raise RuntimeError("Operator console did not stop")

    def stage(self, key, state):
        with self.condition:
            if state == "complete" and self.cancelled.is_set():
                raise Cancelled("Accepted cancellation takes precedence over completion")
            item = next(item for item in self.phases if item["id"] == key)
            item["state"] = state
            self.journal.event("stage", phase=key, state=state)

    def snapshot(self):
        with self.condition:
            prompt = None
            if self.prompt_value:
                prompt = dict(self.prompt_value)
                prompt["remaining"] = max(0, prompt.pop("deadline") - time.monotonic())
            return {"mode": self.mode, "phases": [dict(item) for item in self.phases],
                    "completed": sum(item["state"] in ("complete", "cleaned") for item in self.phases),
                    "prompt": prompt, "result": self.result}

    def respond(self, prompt_id, answer):
        with self.condition:
            if (self.result or self.cancelled.is_set() or not self.prompt_value or self.prompt_value["id"] != prompt_id or
                    self.answer is not None or time.monotonic() >= self.prompt_value["deadline"]):
                raise ValueError("Prompt expired, already answered or replaced")
            if answer != "continue":
                raise ValueError("Unsupported answer")
            self.journal.event("operator_reply", phase=self.prompt_value["phase"], prompt_id=prompt_id)
            self.answer = answer
            self.condition.notify_all()

    def cancel(self):
        with self.condition:
            if self.sealed or self.result or all(item["state"] == "complete" for item in self.phases):
                raise ValueError("Steps complete or run finalizing; cancellation was not accepted")
            self.journal.event("cancel_requested")
            self.cancelled.set()
            self.condition.notify_all()

    def prompt(self, key, message, timeout):
        with self.condition:
            self.stage(key, "waiting")
            self.answer = None
            self.prompt_value = {"id": secrets.token_hex(16), "phase": key,
                                 "message": message, "deadline": time.monotonic() + timeout}
            self.journal.event("prompt", phase=key, prompt_id=self.prompt_value["id"])
            try:
                while self.answer is None:
                    if self.cancelled.is_set():
                        raise Cancelled("Operator cancelled")
                    remaining = self.prompt_value["deadline"] - time.monotonic()
                    if remaining <= 0:
                        raise TimeoutError("Operator confirmation expired")
                    self.condition.wait(remaining)
                if self.cancelled.is_set():
                    raise Cancelled("Operator cancelled")
                self.journal.event("confirmed", phase=key)
            finally:
                self.prompt_value = None

    def seal(self, outcome):
        """Atomic boundary: no accepted cancellation can race report persistence."""
        with self.condition:
            if self.sealed:
                return self.final_outcome
            if outcome == "passed" and self.cancelled.is_set():
                outcome = "cancelled"
            if outcome == "passed" and any(item["state"] != "complete" for item in self.phases):
                outcome = "error"
            self.sealed = True
            self.final_outcome = outcome
            self.journal.event("finalizing", outcome=outcome)
            return outcome

    def finish(self, outcome, detail, *, verification=None):
        with self.condition:
            effective = self.seal(outcome)
            if outcome == "error":
                effective = "error"  # Persistence errors after sealing fail closed.
            self.prompt_value = None
            labels = {"passed": "软件流程通过", "cancelled": "本轮已取消", "timeout": "本轮已超时", "error": "本轮异常"}
            self.result = {"outcome": effective, "label": labels[effective], "detail": detail}
            if verification is not None:
                self.result["verification"] = verification
