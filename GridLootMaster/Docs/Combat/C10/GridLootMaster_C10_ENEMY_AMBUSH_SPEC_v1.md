# GridLootMaster C10 Enemy Ambush 확정 구현 명세 v1

이 문서는 C10에서 Codex가 추가 기획 판단을 하지 않고 구현할 수 있도록 값을 확정한다.

## 1. 범위

구현:
- Ambusher Profile
- Enemy Ambush Trigger
- SEARCH
- COVER
- FLEE
- Surprise Attack 1회 후 기존 Combat 연결

미구현:
- 휴대중량 기반 Mobility
- 여러 적 동시 매복
- 소음 기반 매복
- 복잡한 AI Behavior Framework
- Blueprint / Behavior Tree

## 2. Player v1 값

- PlayerPerception: 현재 GameMode 값 유지
- PlayerMobility: 50 신규 기본값
- CarryWeightPenalty: v1 미적용

## 3. Ambusher 값

FEnemyDefinition 또는 Ambush 관련 최소 데이터:
- AmbushRangeTiles = 1
- AmbushPower = 50

Behavior:
- `EEnemyBehaviorProfile::Ambusher`
- 기본적으로 위치를 유지한다.
- Player가 AmbushRange 안에 들어올 때까지 일반 RandomWander를 하지 않는다.
- LOS가 없으면 Ambush를 발동하지 않는다.

## 4. Enemy Ambush Trigger

다음이 모두 참일 때:
- Raid InRaid
- Combat 없음
- Enemy Alive
- BehaviorProfile == Ambusher
- Enemy가 Hidden 상태
- Distance <= AmbushRangeTiles
- LOS true

그러면 일반 Contact 대신 `Enemy Ambush Reaction` 상태로 진입하고 World Tick을 정지한다.
반응 선택 전에는 자동 피해를 주지 않는다.

Player가 먼저 해당 Ambusher를 탐지/Reveal한 경우 Surprise를 발생시키지 않고 기존 Normal Contact 규칙을 사용한다.

## 5. SEARCH

확률:

`Clamp(50 + PlayerPerception - EnemyStealth - AmbushPower, 10, 90)`

기본값:
PlayerPerception 50 / EnemyStealth 0 / AmbushPower 50 -> 50%

성공:
- Enemy Revealed
- Surprise 제거
- Normal Combat 시작
- 별도 무료 선제공격 없음

실패:
- Enemy Surprise Attack 1회
- 이후 Normal Combat

## 6. COVER

COVER 자체에는 추가 RNG를 사용하지 않는다.

CoverValue는 Map v1의 내부 Edge Wall 개수로 결정한다.
Boundary는 Cover 계산에 포함하지 않는다.

- 내부 Wall 0개 -> Open -> 0
- 내부 Wall 1개 -> Light -> 25
- 내부 Wall 2개 -> Medium -> 50
- 내부 Wall 3개 이상 -> Heavy -> 75

제공 데이터:
`GridLootMaster_MapCover_v1.csv`

효과는 **Enemy Surprise Attack 1회에만** 적용:

`FinalDamage = ceil(IncomingDamage * (100 - CoverValue) / 100)`

그 뒤 Normal Combat.

Cover는 일반 전투 전체에 지속되는 방어 Buff로 만들지 않는다.

## 7. FLEE

확률:

`Clamp(50 + PlayerMobility - AmbushPower, 10, 90)`

기본값:
Mobility 50 / AmbushPower 50 -> 50%

성공:
- Combat 시작하지 않음
- Player를 직전 정상 Tile로 복귀
- 현재 Ambush Reaction 종료
- Enemy는 살아 있는 World Enemy로 유지

실패:
- Enemy Surprise Attack 1회
- 이후 Normal Combat

이를 위해 GameMode는 최소한 `PreviousPlayerCoord`를 안전하게 기억해야 한다.
FLEE 성공 목적지는:
1. 유효 좌표
2. 현재 Player 위치와 인접한 이전 Tile
3. Wall 규칙상 실제 이동 가능한 Tile
이어야 한다.
조건이 깨졌다면 FLEE 성공을 강제로 만들지 말고 실패/안전 취소 처리한다.

## 8. Surprise Attack

기존 Enemy 공격 계산을 복제하지 않는다.
기존 `EnemyAttackPlayer()` 또는 동등한 공통 Damage 경로를 재사용한다.

COVER만 첫 Surprise Attack의 Damage modifier를 전달할 수 있도록 최소 확장한다.

## 9. 테스트

필수 Targeted:
- Ambusher가 Range 밖이면 반응 없음
- Wall/LOS 차단 시 반응 없음
- Revealed Ambusher는 Surprise 없음
- Hidden Ambusher + Range1 + LOS -> Ambush Reaction
- 반응 대기 중 World Tick 정지
- SEARCH 성공 -> 피해 없이 Normal Combat
- SEARCH 실패 -> Surprise 1회 -> Combat
- COVER 0/25/50/75 피해 감소
- FLEE 성공 -> PreviousPlayerCoord 복귀, Combat 없음
- FLEE 실패 -> Surprise 1회 -> Combat
- Player death from Surprise -> 기존 FailRaid
- C10 완료 후 Full GridLootMaster Test 1회
