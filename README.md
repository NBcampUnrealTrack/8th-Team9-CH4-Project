# MeowTractive

TPS 캐주얼 액션 / 경쟁형 멀티플레이 / 파티 게임 / 탑뷰 익스트랙션
(Casual Action · Competitive Multiplayer · Party · Top-down Extraction)

- **엔진**: Unreal Engine **5.8**
- **런타임 모듈**: `MeowTractive` (C++)
- **사용 플러그인**:
  - `GameplayAbilities` — GAS(캐릭터·NPC 능력/스탯)
  - `OnlineSubsystem` + `OnlineSubsystemSteam` — Steam 세션(SteamOss)
  - `CommonUI` — UI 프레임워크
  - `AsyncLoadingScreen` — 로딩 화면(마켓플레이스)
  - `ModelingToolsEditorMode` — 모델링 도구(Editor 전용)

---

## 개발 환경

| 항목 | 권장 |
|------|------|
| 엔진 | Unreal Engine 5.5 |
| IDE | Visual Studio 2022 / JetBrains Rider |
| 버전 관리 | Git + **Git LFS (파일 잠금)** |
| 소스 인코딩 | **UTF-8 with BOM** (`.editorconfig` 적용 — 한글 주석 깨짐 방지) |

---

## 빠른 시작

```bash
# 1. 클론
git clone <repo-url>
cd MeowTractive

# 2. LFS 최초 1회 설정
git lfs install
git lfs pull          # 에셋이 1KB 텍스트 스텁으로 보이면 반드시 실행
```

3. `MeowTractive.uproject` **우클릭 → Generate Visual Studio project files**
4. `MeowTractive.sln`을 VS2022/Rider에서 열고 **Development Editor | Win64**로 빌드
5. 빌드 성공 후 `.uproject` 더블클릭으로 에디터 실행

> C++ 파일을 새로 받거나 `*.Build.cs`가 바뀌면 **Generate Project Files**를 다시 실행하세요.

---

## 프로젝트 구조

```
Source/MeowTractive/   # C++ 런타임 모듈
Content/Main/      # 정식 에셋 (Git LFS, Check Out 필수)
Content/Developers/    # 개인 샌드박스 (잠금 없음, 자유 작업)
Config/                # 프로젝트 설정 (추적 대상)
```

생성물(`Binaries/`, `Intermediate/`, `Saved/`, `DerivedDataCache/`)은 `.gitignore` 처리되어 커밋되지 않습니다.

---

## ⚠️ 협업 시 주의사항 (필독)

### 1. 에디터 On/Off

- **Pull 전에는 언리얼 에디터를 끄세요.** 로드된 `.uasset`/`.umap`을 pull로 덮으면 충돌이 나고, 에디터가 그 위에 저장하면 받은 변경이 날아갑니다.
- **Push 전에는 Save All과 에디터 종료** 후 커밋하세요.
- **C++ 변경을 받은 뒤**에는 에디터를 끈 상태로 VS에서 빌드한 뒤 다시 실행하세요.

### 2. Git LFS / 파일 잠금

- `.uasset`/`.umap` 편집 전 **에디터에서 Check Out(잠금)** 을 먼저 합니다.
- Push하면 잠금이 자동 해제됩니다. 수동 해제는 `git lfs unlock <path>`.
- **남의 잠금을 강제 해제(`--force`)하지 마세요.** 반드시 당사자와 협의합니다.
- 작업 시작 전 `git pull`로 최신화하고 `git lfs locks`로 잠금 현황을 확인하세요.

### 3. 에셋 이동 / 폴더 규칙

- `Content/Developers/`(샌드박스)에서 `Content/Main/`(정식)로 **cross-reference 금지** — 샌드박스 정리 시 참조가 깨집니다.
- 에셋 이동·이름변경은 **반드시 에디터에서** 하고, 옮긴 폴더에 **Fixup Redirectors**를 실행한 뒤 커밋하세요. (탐색기에서 직접 옮기면 참조가 깨집니다.)

---

## 브랜치 / 커밋 규칙

- **개인 브랜치 금지** (`dev/<이름>` 사용 안 함)
- **C++ 작업**: `feature/<name>` → `main`으로 **PR**. *Push 전 로컬 컴파일 성공 필수.*
- **블루프린트 / 맵 / 데이터 작업**: 바이너리는 머지가 불가능하므로 `develop`에 **직접 커밋**하고, 충돌은 LFS 잠금으로 방지합니다.

### 커밋 메시지 형식

```
[TAG] <설명> (Jira 티켓 선택)
```

| 태그 | 용도 |
|------|------|
| `[ADD]`  | 새 파일/에셋 추가 |
| `[FEAT]` | 새 로직 |
| `[FIX]`  | 버그 수정 |
| `[UPD]`  | 수치/밸런스 등 조정 |
| `[DOC]`  | 문서/주석 |

---

## ⚠️ 리플리케이션 주의사항 (멀티플레이)

본 프로젝트는 **경쟁형 멀티플레이 + Steam 세션(SteamOss)** 기반이므로, 게임플레이 C++ 코드는 **서버 권위(Server-Authoritative)** 원칙을 따릅니다.

### 1. 권위(Authority) 원칙

- **게임 상태를 바꾸는 로직은 서버에서만 실행하세요.** 체력 감소, 점수, 아이템 획득, 사망 처리 등은 서버에서 판정하고 클라에 복제합니다.
- 분기 전에 `HasAuthority()`(또는 `GetLocalRole() == ROLE_Authority`)로 권위를 확인하세요.
- **연출(Cosmetic)** — 사운드, 파티클, UI 애니메이션 등 게임 결과에 영향이 없는 처리는 클라에서 해도 됩니다.

### 2. 변수 복제

- 복제할 변수는 `UPROPERTY(Replicated)` 또는 `UPROPERTY(ReplicatedUsing=OnRep_Xxx)`로 선언하고, `GetLifetimeReplicatedProps()`에서 `DOREPLIFETIME` 매크로로 등록하세요.
- **`OnRep_` 함수는 클라이언트에서만 자동 호출됩니다.** 서버에서도 동일 처리가 필요하면 값 변경 직후 `OnRep_Xxx()`를 **직접 호출**하세요.
- 액터가 복제되려면 `AActor::bReplicates = true`. 이동 복제는 `bReplicateMovement` 또는 `CharacterMovementComponent`에 위임합니다.

### 3. RPC

- **Server RPC**: 클라 → 서버 요청. `WithValidation`으로 입력을 검증해 치팅을 방어하세요.
- **Client RPC**: 서버 → 특정 클라. **Multicast RPC**: 서버 → 모든 클라(연출 동기화용, 남용 금지).
- 빈번하거나 손실되어도 되는 호출은 `Unreliable`, 반드시 도달해야 하면 `Reliable`. **Reliable 남발은 네트워크 큐 오버플로**의 원인이 됩니다.
- RPC는 **복제되는 액터에서만** 정상 동작합니다(소유권/Net Relevancy 확인).

### 4. GAS 연동

- `GameplayAbility`는 **서버에서 활성화**하고 예측(Prediction)으로 클라 반응성을 확보합니다.
- `AttributeSet`/`AbilitySystemComponent`는 복제 설정이 필요하며, 시각 효과는 변수 복제 대신 **GameplayCue**로 처리하세요.

### 5. 테스트

- PIE의 **Net Mode = Play As Listen Server / Client**, **Number of Players ≥ 2**로 항상 멀티 환경에서 검증하세요.
- 단일 플레이에서만 동작하는 코드는 복제 누락일 가능성이 높습니다. **서버/클라 양쪽에서** 동작을 확인하세요.

---

## 설계 방침 — GAS 기반 캐릭터

플레이어와 NPC의 능력·스탯·상태는 **Gameplay Ability System(GAS)** 으로 설계합니다. (`GameplayAbilities` 플러그인 사용)

- **플레이어(고양이)**: 이동·공격·상호작용 등 행동은 `GameplayAbility`로, 체력·스태미나 등 수치는 `AttributeSet`으로 구현합니다. 직접 함수 호출 대신 **GameplayTag + Ability 활성화** 패턴을 사용하세요.
- **행인 AI(NPC)**: 행인도 동일하게 `AbilitySystemComponent`를 보유하고, AI 컨트롤러(BT/State)는 **상태 전이 시 Ability를 활성화**하는 방식으로 동작을 트리거합니다. 매료 등 반응은 `GameplayEffect`/`GameplayCue`로 처리합니다.
- 플레이어와 행인이 **같은 GAS 인터페이스(Ability·Effect·Tag)** 를 공유하도록 설계해, 상호작용이 한 경로로 흐르게 합니다.
- 멀티플레이 복제는 위 [리플리케이션 주의사항](#️-리플리케이션-주의사항-멀티플레이)의 **GAS 연동** 항목을 따르세요(서버 권위 활성화 + 예측, 시각효과는 GameplayCue).

---

## 코드 컨벤션 요약

- 게임플레이 코드는 `MT` 접두사 + `Public/<Subsystem>/` · `Private/<Subsystem>/` 미러 구조
- `UPROPERTY` 카테고리는 `"MT|<Subsystem>"` (예: `MT|Stat`, `MT|Weapon`)
- 주석은 한글 기본, 식별자는 영문(언리얼 컨벤션)

---

## 아키텍처 — 단일책임(SRP) / 레이어 분리

스파게티 방지를 위해 **레이어별 단일 책임**을 지킵니다.

1. **위젯엔 게임/네트워크 로직·트래블 금지** — 상태 읽기 + 의도 전달만.
2. **트래블 책임**: 서버측 = `GameMode`, 메뉴/클라측 = `Flow(UMTGameInstance)`. 위젯·서브시스템엔 두지 않음.
3. **레이어 간 통신은 델리게이트/인터페이스** — 하드 참조 최소. (`UMTSessionSubsystem`이 BP 비호환 OSS 타입을 캡슐화하는 방식 유지)
4. **"한 클래스 = 한 변경 이유"** — 새 책임이 생기면 새 클래스.

| 레이어 | 책임 | 트래블 |
|--------|------|--------|
| `UMTSessionSubsystem` | 세션 I/O(생성/검색/참가/파괴) | ❌ |
| `UMTGameInstance` (Flow) | 세션 이벤트 구독 → 메뉴/클라 트래블 | ✅ |
| `GameMode` | 서버 권한·규칙 | ✅(서버) |
| `PlayerController` | 입력 → 서버 RPC | ❌ |
| Widget(UMG) | 표현·상태 표시·의도 전달 | ❌ |
