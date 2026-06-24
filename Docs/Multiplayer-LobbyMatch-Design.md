# 멀티플레이 로비→매치 플로 설계 (MeowTractive)

> 리슨 서버 + Steam OSS(**네이티브 C++**, `UMTSessionSubsystem`) 기준. 최대 4인.
> 플로: 스플래시 → 메인메뉴 → 로비 → 전환맵(비동기 로딩) → 인게임. 타임아웃/예외 포함.

---

## 0. 원칙

- **모든 상태 변경은 서버 권한**(`HasAuthority`). 클라는 Server RPC로 요청.
- 세션/매치/인원 데이터는 **GameState·PlayerState로 복제**, 매치 진행 로직은 **GameMode(서버 전용)**.
- 맵마다 전용 GameMode: `MainMenu`=`BP_MainMenuMode`, `Lobby`=`AMTLobbyGameMode`, `GameLevel`=`AMTMatchGameMode`.

---

## 1. 클래스 책임

| 클래스 | 베이스 | 역할 |
|--------|--------|------|
| `UMTSessionSubsystem` | `UGameInstanceSubsystem` | 세션 생성/검색/참가/파괴 + **`UpdateSession()`(잠금)** 추가 |
| `AMTLobbyGameMode` | `AGameModeBase` | 로비 입장/퇴장, 시작 게이팅, 세션 잠금 후 전환 트리거 |
| `AMTMatchGameMode` | **`AGameMode`** | 매치 상태머신(`bDelayedStart`/`ReadyToStartMatch`/`StartMatch`), `PostLogin` Player Index 부여, 60초 타임아웃·kick |
| `AMTGameState` | `AGameState` | 매치 단계, 플레이어 로스터, 팀/적 정보, 남은 시간 — **복제** |
| `AMTPlayerState` | `APlayerState` | `SelectedCatType`, `bIsReady`, `bIsLoaded`, `PlayerIndex`, `TeamId` — **복제** + `CopyProperties` |
| `AMTPlayerController` | `APlayerController` | 준비 토글(0.5s 쿨다운), 입력 차단, 로딩/로비 UI 갱신 |
| `AMTCatCharacter` | `ACharacter` | 고양이 폰(종류별). 선택 결과로 스폰. (GAS 연동) |

---

## 2. 데이터 모델 (복제 위치)

### `AMTPlayerState` (플레이어별, seamless travel 유지 대상)
```cpp
UPROPERTY(ReplicatedUsing=OnRep_SelectedCat) EMTCatType SelectedCatType;
UPROPERTY(ReplicatedUsing=OnRep_Ready)       bool bIsReady = false;
UPROPERTY(Replicated)                         int32 PlayerIndex = -1;
UPROPERTY(Replicated)                         int32 TeamId = -1;     // 개인전 = -1
UPROPERTY(Replicated)                         bool bIsLoaded = false; // 레벨마다 리셋
```
- `GetLifetimeReplicatedProps`에 전부 등록.
- **`CopyProperties` 오버라이드**: `SelectedCatType`, `TeamId`(, 필요시 `PlayerIndex`)를 복사 → seamless travel 시 선택 유지. `bIsLoaded`는 **복사 안 함**(레벨마다 새로 보고).

### `AMTGameState`
```cpp
UPROPERTY(Replicated) EMTMatchPhase Phase;         // Lobby/WaitingLoad/InProgress/PostMatch
UPROPERTY(Replicated) TArray<TObjectPtr<AMTPlayerState>> Players;
UPROPERTY(Replicated) float MatchTimeRemaining;
```

### `AMTMatchGameMode` (서버 전용, 복제 X)
- 타임아웃 타이머 핸들, 로딩 보고 카운트, bDelayedStart 등.

---

## 3. 플로 상세 (네이티브 매핑)

### Phase 1 — 실행/초기화  *(대부분 구현됨)*
- 스플래시 → 메인메뉴(Standalone). OSS(Steam) 초기화.
- 호스트: `UMTSessionSubsystem::CreateSession` → 성공 시 `ServerTravel(Lobby?listen)`.
- 클라: `FindSessions` → `JoinSession` → 자동 client travel.

### Phase 2 — 로비 (`Lobby` 맵 / `AMTLobbyGameMode`)
- 입장 시 `PostLogin`에서 PlayerState 초기화, GameState 로스터 갱신.
- **캐릭터 선택**: 클라 → `Server_SetSelectedCat(EMTCatType)` (서버에서 PlayerState 갱신 → 복제).
- **준비 토글**: 클라 → `Server_ToggleReady()`. 서버에서 **0.5s 쿨다운** 검증(`WithValidation` 또는 서버 타임스탬프). `bIsReady` 동안 캐릭터 선택/나가기 UI 비활성(클라 OnRep에서).
- **시작 버튼(호스트)**: 모든 클라 `bIsReady && 최소 1명` 일 때만 활성. 클릭 → `Server_RequestStartMatch()`.

### Phase 3 — 시작 전환 (Async Loading Screen 플러그인)
> **로딩 화면**은 **Async Loading Screen 플러그인**이 `PreLoadMap`/`PostLoadMapWithWorld` 훅으로 **트래블 중 자동 표시**한다. → **별도 Trans_Map/수동 로딩 UI 불필요.**

서버:
1. **세션 잠금**: `UMTSessionSubsystem::UpdateSession(bAllowJoinInProgress=false)`.
2. **입력 차단**: 전원 PlayerController에 "전환 중" 플래그 복제 → 이동/나가기 입력 무시.
3. **게임맵으로 직행**: `ServerTravel("GameLevel?listen")`. 트래블 중 플러그인이 로딩 화면을 깐다.

> ⚠️ 플러그인은 **로딩 화면(UI)만** 담당한다. **플레이어 동기화(전원 로딩 완료 후 동시 시작)는 안 한다** → 그건 **Phase 4의 `bIsLoaded` + `ReadyToStartMatch`** 가 담당.
> **(옵션·성능용)** `GameLevel`이 매우 무거워 로딩이 길면, 중간에 가벼운 `Trans_Map`을 두고 거기서 `LoadPackageAsync`로 `GameLevel`을 프리로드(에셋 워밍)한 뒤 `ServerTravel`할 수 있다. 프리로드 패키지는 `FStreamableHandle`로 GC 방지. 보통은 플러그인만으로 충분하므로 **기본은 직행**.

### Phase 4 — 인게임 진입 (`GameLevel` / `AMTMatchGameMode : AGameMode`)
- `bDelayedStart = true` → 매치는 `WaitingToStart`에서 대기.
- **로딩 동기화 (핸드셰이크 방식)**: 서버가 **`HandleSeamlessTravelPlayer`**(seamless) / **`PostLogin`**(non-seamless)에서 각 플레이어 `bIsLoaded=true` 설정 → **별도 클라 RPC 불필요**. `bIsLoaded`는 클라에 복제되어 "로딩 중/Ready" UI에 사용.
- `PostLogin`마다: **Player Index 부여** + GameState 로스터 갱신 → 상세는 **§3-A**.
- **`ReadyToStartMatch()` 오버라이드**: **`NumTravellingPlayers == 0`**(이동 중 플레이어 없음) + 전원 `bIsLoaded` → `true`.
- 충족 시 `StartMatch()` → `MatchState=InProgress`. 클라는 `GameState->IsMatchInProgress()`로 입력/UI 게이트.

### Phase 5 — 타임아웃/예외
- **60초 타임아웃**: `HandleMatchIsWaitingToStart()`에서 서버 타이머 시작.
- **클라 타임아웃**: 초과 클라 `GameSession->KickPlayer` → 남은 인원으로 `StartMatch` 강제.
- **호스트 증발**: 클라에서 `GEngine`/`FCoreDelegates` **`OnNetworkFailure`** 바인딩 → 로컬에서 `OpenLevel(MainMenu)` 자동 복귀.

### 3-1. Player Index & 매료도/타게팅 데이터  (핵심 메커닉 연결)

**① Player Index 부여**
- 서버 `PostLogin`(또는 로비 입장)에서 빈 슬롯 `0~3` 할당 → `AMTPlayerState::PlayerIndex` 복제. 퇴장 시 인덱스 회수.
- 용도: 스폰 슬롯 매핑, 스코어보드/UI 색·순서, **행인 매료도 배열의 키**, 친구/적 판별.

**② GameState 로스터 전달**
- `AMTGameState`가 현재 플레이어 목록(`PlayerIndex` + `TeamId`)을 복제 → **전 클라가 "몇 명·어느 팀"을 공유**.

**③ 매료도/타게팅 데이터 생성** (게임 핵심과 직결)
- 행인(NPC)은 **플레이어별 매료도(0~100)** 를 가짐 → `TArray<float> CharmByPlayer` 를 `Players.Num()` 크기로 초기화(배열 인덱스 = `PlayerIndex`).
- 고양이 어빌리티 **타게팅**: GameState 로스터로 **적**(개인전=자신 외 전원 / 팀전=다른 `TeamId`)을 판별.
- 매료 경쟁/탈취 판정도 이 `PlayerIndex` 키로 누가 100을 먼저 채웠는지 추적.

**④ 타이밍**
- 로스터는 **`StartMatch` 직전 확정**(이후 join-in-progress 잠금으로 인원 고정).
- 행인 스폰 및 `CharmByPlayer` 배열 초기화는 **`StartMatch` 이후** 서버에서.

---

## 4. Seamless Travel 설계

- `AMTLobbyGameMode`/`AMTMatchGameMode`: **`bUseSeamlessTravel = true`** (생성자).
- Project Settings → Maps & Modes → **Transition Map** = 가벼운 `Trans_Map` 지정.
- **유지**(CopyProperties로 복사): `SelectedCatType`, `TeamId`. → 로비 선택이 인게임까지 감.
- **리셋**(복사 안 함): `bIsLoaded`(레벨마다 새 보고), `bIsReady`(인게임 의미 없음).
- 주의: seamless travel은 PlayerController/PlayerState를 **유지**하지만, GameMode 클래스가 다르면 PC는 새 GameMode의 클래스로 교체됨.

---

## 5. RPC / 권한 요약

| 동작 | 방향 | 함수 | 검증 |
|------|------|------|------|
| 캐릭터 선택 | Client→Server | `Server_SetSelectedCat` | 로비 단계만 허용 |
| 준비 토글 | Client→Server | `Server_ToggleReady` | **0.5s 쿨다운**(서버 시간) |
| 시작 요청 | Host→Server | `Server_RequestStartMatch` | 호스트 + 전원 Ready |
| 로딩 완료 | 서버 내부 | `HandleSeamlessTravelPlayer`/`PostLogin` | 엔진 핸드셰이크(RPC 없음) |
| UI 갱신 | 서버→클라 | OnRep / `Client_` RPC | — |

---

## 6. GAS 연동 (요약)

- **ASC 위치: PlayerState (확정)** — 리스폰 잦음 → 어트리뷰트/매료도/태그 유지. Pawn 빙의 시 `InitAbilityActorInfo` 재설정.
- 캐릭터 선택(`SelectedCatType`) → `AMTMatchGameMode`가 해당 고양이 폰 스폰·Possess.
- 어빌리티(애교/눈빛/대시/특수)는 GA, 스텟·쿨다운은 GE/AttributeSet.

---

## 7. 결정 사항 (확정)

- ✅ **ASC = PlayerState** (리스폰 잦음 → 어트리뷰트/매료도/태그 유지)
- ✅ **개인전(FFA) 우선**. `TeamId`는 필드만 남기고 `-1` 고정 (팀전은 나중에 활성)
- ✅ **로딩 화면 = Async Loading Screen 플러그인** (트래블 중 자동 표시) → 별도 Trans_Map 불필요. 기본은 `ServerTravel` 직행. (GameLevel이 무거우면 Trans_Map+`LoadPackageAsync` 프리로드는 선택)
- ⏳ **캐릭터 선택지/종류 = 미정** — 별도 담당, 본 작업 범위 밖.
- ✅ **입력 차단 = "전환 중" 플래그 복제** (PlayerController가 OnRep에서 입력 무시)

---

## 8. 구현 마일스톤

- **M1. 로비 기반** — `AMTPlayerState`(선택·준비·CopyProperties), `AMTLobbyGameMode`, 로비 UI, 준비 토글/시작 게이팅.
- **M2. 전환** — `UpdateSession`(잠금) + 입력 차단 + seamless `ServerTravel("GameLevel?listen")`. 로딩 화면은 Async Loading Screen 플러그인이 처리(Trans_Map/프리로드는 선택).
- **M3. 매치 진입** — `AMTMatchGameMode : AGameMode`(bDelayedStart/ReadyToStartMatch/StartMatch) + `bIsLoaded` 동기화 + PlayerIndex/GameState.
- **M4. 예외** — 60초 타임아웃·kick + `OnNetworkFailure` 메뉴 복귀.

> 각 마일스톤은 **Null+LAN 2-인스턴스**로 검증 후 다음으로.
