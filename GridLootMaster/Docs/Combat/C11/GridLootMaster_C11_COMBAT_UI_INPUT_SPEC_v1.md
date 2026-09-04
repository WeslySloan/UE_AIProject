# GridLootMaster C11 Combat UI / Input 확정 명세 v1
## C++ WidgetTree Only

## 1. 목표

이미 구현된 Gameplay API를 새로 설계하지 않고 실제 플레이 가능한 UI/Input으로 연결한다.

현재 Source에 존재:
- `BANG!` -> Player Attack 경로
- 1/2 -> Weapon Swap
- R -> Reload
- Player Ambush API
- CombatText
- 상단 Event Notification Queue

C11은 Gameplay Source of Truth를 UI에 복제하지 않는다.

## 2. 우측 Combat Panel

기존 RightPanel의 `CombatText`와 `BANG` 영역을 재구성하되,
Inventory/Container/Stash 구조를 파괴하지 않는다.

Combat 활성 시 최소 표시:

- ENEMY
  - 이름
  - HP / MaxHP
  - 거리(Tile)
- ACTIVE WEAPON
  - ItemName
  - Ammo Current / Max (Firearm일 때)
- ATTACK
  - READY 또는 Cooldown 남은 시간
  - 현재 Recoil
- ACTION
  - SWAPPING / RELOADING / READY
  - 행동 남은 시간

표시값은 CombatComponent/UItemInstance에서 읽는다.

## 3. 공격 입력

전역 화면 전체 LMB를 가로채지 않는다.

현재 UI는 Inventory Drag, Equipment, Minimap, Context Menu 모두 LMB를 사용하므로
`NativeOnPreviewMouseButtonDown()`에서 모든 LMB를 Fire로 처리하는 방식은 금지한다.

v1 권장:
- Combat Panel 안에 `CombatFireSurface` 또는 기존 FIRE/BANG Button을 충분히 큰 명시적 공격 영역으로 사용
- 해당 영역의 LMB -> 기존 `OnBangButtonClicked()` 또는 공통 `RequestAttackFromUI()`
- Inventory/Equipment/Minimap/Button/Modal 입력은 기존대로 동작

추후 전역 LMB 공격이 필요하면 PlayerController InputMode와 Slate Hit Path를 별도 설계한다.

## 4. BANG 명칭

기능 확인용 `BANG!`은 최종 UI에서:
- Firearm: `FIRE`
- Melee: `ATTACK`

으로 동적 표시한다.

공통 Gameplay 호출은 유지한다.

## 5. Reload

R 입력은 이미 연결되어 있으므로 유지.

추가 표시:
- `RELOADING 1.2s`
- 완료 시 자동으로 READY
- 실패 시 기존 상단 Event Notification 사용

별도 Reload Button은 키보드 입력 접근성을 위해 선택적으로 둘 수 있지만 필수 아님.

## 6. Weapon Swap

1 / 2 입력 유지.

전투 중:
- 요청 직후 UI Active Slot을 바꾸지 않음
- `SWAPPING 0.xs`
- Combat 완료 이벤트 후 실제 ActiveWeaponSlot 갱신

현재 구현 원칙을 그대로 시각화한다.

## 7. Recoil / Cooldown

숫자 텍스트 우선.

v1:
- `READY`
- `COOLDOWN 0.34s`
- `RECOIL 18`

진행바/애니메이션은 필수가 아니다.
C++만으로 간결하게 구현한다.

## 8. Player Ambush UI

비전투 / 이동 중 아님 / InRaid일 때:
- `[AMBUSH]`

Ambushing:
- 상태 `AMBUSHING`
- `[WAIT]`
- `[CANCEL]` 또는 기존 상태 해제 API가 없다면 C11에서 최소 Cancel API 추가

의심 적이 접근:
- `TARGET APPROACHING`
- `[LET PASS]`
- `[ASSAULT]`

Gameplay API:
- RequestPlayerAmbush
- RequestAmbushWait
- RequestAmbushLetPass
- RequestAmbushAssault

를 재사용한다.

## 9. Enemy Ambush Reaction UI

C10 완료 후에만 연결:

`AMBUSHED`

버튼 3개:
- SEARCH
- COVER
- FLEE

Reaction 상태가 아닐 때 숨긴다.
한 번 선택 후 중복 입력을 막는다.

## 10. 알림

기존 상단 Event Notification Queue를 재사용.

예:
- 공격 쿨다운
- 사거리 밖
- 탄약 없음
- Reload 실패
- Enemy Contact
- Ambush 실패/성공
- Flee 성공/실패

새 Notification Manager를 만들지 않는다.

## 11. 입력 회귀 필수

C11 변경 후 반드시 수동/자동 확인:
- Inventory Drag
- Equipment Drag
- Mod Slot
- Ammo Drag
- Minimap Tile Click
- Compact Minimap
- Context Menu
- Split Stack Modal
- Stash UI
- FIRE 영역 외 LMB가 발사를 유발하지 않음

## 12. C11 완료 조건

- 공격 UI
- 공격 Cooldown 시각화
- Recoil 시각화
- Reload 상태
- Swap 상태
- Player Ambush UI
- Enemy Ambush Reaction UI(C10 완료 후)
- 기존 UI 입력 회귀 없음
- Development Editor Compile
- 관련 UI/Combat Targeted Test
- Full GridLootMaster Test 1회
- 이후 실제 Editor Manual Raid QA
