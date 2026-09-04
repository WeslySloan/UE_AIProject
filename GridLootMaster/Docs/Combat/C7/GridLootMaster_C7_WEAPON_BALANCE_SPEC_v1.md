# GridLootMaster C7 무기 밸런스·DataTable 임포트 명세 v1

기준:
- 최신 Source `ItemData.h`, `ItemInstance`, `CombatComponent`, `MainGameUI`
- 사용자 제공 `ItemData(1).csv` 33행
- 현재 일반 WorldScav 기준 HP 100
- 아래 수치는 **초기 프로토타입 밸런스 확정안**이며 실제 플레이 후 조정한다.

## 1. 제공 산출물

`GridLootMaster_ItemData_C7_BALANCED_v1.csv`

- 기존 33행을 보존한다.
- 기존 컬럼을 삭제하지 않는다.
- Source에 이미 추가된 C7 컬럼 10개를 추가한다.
- 비무기 행에는 현재 C++ 안전 기본값과 동일한 값을 넣는다.
- M4A1 Damage는 현재 자동화 테스트 기대값 25를 보존한다.

## 2. 무기 초기값

| 무기 | Damage | Acc | Interval | Optimal | Max | Recoil | Recovery/s | Swap | Reload | Noise | 100HP 최소 명중수 | 이론 최소 TTK | 역할 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|
| Glock19 | 20 | 90 | 0.70s | 1 | 2 | 8.0 | 8.0 | 0.35s | 1.30s | 3 | 5 | 2.80s | 빠른 스왑·안정적인 근거리 보조무기 |
| MP5 | 18 | 84 | 0.40s | 1 | 3 | 13.0 | 12.0 | 0.55s | 1.60s | 4 | 6 | 2.00s | 고연사 근중거리, 연사 시 반동 누적이 빠름 |
| AK74M | 28 | 80 | 0.70s | 2 | 3 | 18.0 | 12.0 | 0.75s | 1.90s | 5 | 4 | 2.10s | 강한 단발·높은 반동의 중거리 AR |
| M4A1 | 25 | 88 | 0.60s | 2 | 3 | 13.0 | 14.0 | 0.70s | 1.80s | 5 | 4 | 1.80s | 명중률·연사·반동이 균형 잡힌 범용 AR |
| Mosin | 45 | 94 | 1.40s | 3 | 5 | 24.0 | 18.0 | 0.90s | 2.40s | 7 | 3 | 2.80s | 최장거리·고단발, 매우 느린 후속 공격 |

`이론 최소 TTK`는 모든 탄이 명중하고 첫 공격이 즉시 가능한 경우의 `(필요 명중수 - 1) × AttackInterval` 값이다.
실제 TTK는 명중 실패와 누적 반동 때문에 더 길어져야 한다.

## 3. 반동 의도

현재 Source는 `EffectiveAccuracy = BaseAccuracy - CurrentRecoil`을 사용하고,
발사 후 `CurrentRecoil += RecoilPerShot`,
시간 경과 후 `RecoilRecoveryPerSecond × DeltaTime`만큼 회복한다.

따라서:
- Glock: 빠른 연속 사격에서도 반동 증가가 완만
- MP5: 최대 연사 시 명중률이 빠르게 떨어짐
- AK74M: 강한 단발이지만 연속 사격 패널티가 큼
- M4A1: AR 중 가장 안정적
- Mosin: 다음 사격까지 반동이 거의 회복되어 단발 정밀성이 유지

## 4. OptimalRange 미연결 주의

현재 Source는 `RequestPlayerAttack()`에 `MaxRangeTiles`만 전달한다.
`OptimalRangeTiles`는 데이터에는 존재하지만 실제 명중 계산에는 아직 사용되지 않는다.

C7 완료 전에 권장 최소 연결:
- Distance <= OptimalRangeTiles: 추가 거리 패널티 없음
- OptimalRangeTiles < Distance <= MaxRangeTiles:
  - 초과 1 Tile마다 Accuracy -15
- Distance > MaxRangeTiles: 기존처럼 공격 거부

이 변경은 CombatComponent에서 거리 계산을 단일 Source of Truth로 처리한다.
UI가 독자적으로 명중률을 계산하지 않는다.

## 5. 현재 콘텐츠 공백 — C7 완료 전에 판단 필요

최신 ItemData에는 총기 5개가 있지만 Magazine 행은 `Mag_M4` 하나뿐이다.

현재 Source의 Firearm 공격 규칙:
`Firearm -> EquippedMagazine 필수 -> CurrentAmmo > 0`

따라서 현재 콘텐츠 그대로라면 정상 플레이 흐름에서:
- M4A1: `Mag_M4` + 5.56 ammo 사용 가능
- Glock19: 9mm 탄약은 있으나 9mm Magazine 행이 없음
- MP5: 9mm 탄약은 있으나 9mm Magazine 행이 없음
- AK74M: 5.45 탄약은 있으나 5.45 Magazine 행이 없음
- Mosin: 7.62x54R Magazine과 Ammo 행 모두 없음

또한 현재 Mod/Equipment UI는 `AttachmentType == Magazine`이면 무기에 장착할 수 있으며,
Weapon의 `CompatibleAmmo`와 Magazine의 `CompatibleAmmo`를 서로 검증하지 않는다.
즉 잘못된 구경의 Magazine을 다른 총에 장착할 수 있는 경로가 남아 있다.

### 권장 처리 순서

C7 DataTable 임포트와 별개로 다음 최소 Content Patch를 추가한다.

1. 9x19mm Magazine 추가
2. 5.45x39mm Magazine 추가
3. 7.62x54R Magazine/또는 Mosin 전용 탄창 규칙 추가
4. 7.62x54R Ammo 추가
5. Magazine 장착 시 Weapon-CompatibleAmmo와 Magazine-CompatibleAmmo 일치 검사

이것은 신규 대형 시스템이 아니라 현재 5개 Firearm이 실제로 사용 가능하도록 만드는 콘텐츠 완결 작업이다.

## 6. Codex 임포트·검증

1. 현재 DT_ItemData를 백업/현재 Source와 대조
2. `GridLootMaster_ItemData_C7_BALANCED_v1.csv` 재임포트
3. Unreal DataTable import 0 Problems 확인
4. 다음 Targeted Test:
   - C7 필드가 UItemInstance로 복사
   - M4A1 Damage 25 유지
   - 5개 Weapon의 Interval/Range/Recoil 값이 CSV와 일치
5. OptimalRange 거리 페널티를 연결했다면 Range Targeted Test 추가
6. Phase 종료 시 Full GridLootMaster Automation Test 1회

작은 데이터 수정마다 Full Test를 반복하지 않는다.
