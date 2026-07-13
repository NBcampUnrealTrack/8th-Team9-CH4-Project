import unreal

# MCP 서버 자동 시작: 인터랙티브 에디터 전용.
# 쿠커 등 커맨드렛(-run=)에서 띄우면 에디터와 8000 포트가 충돌해
# LogHttpListener Error로 쿡이 실패하므로 반드시 건너뛴다.
# (Editor Preferences의 Auto Start Server는 커맨드렛을 구분하지 못해 꺼둔 상태)
if "-run=" not in unreal.SystemLibrary.get_command_line().lower():
    unreal.SystemLibrary.execute_console_command(None, "ModelContextProtocol.StartServer")
    unreal.log("init_unreal.py: ModelContextProtocol.StartServer (interactive editor only)")
