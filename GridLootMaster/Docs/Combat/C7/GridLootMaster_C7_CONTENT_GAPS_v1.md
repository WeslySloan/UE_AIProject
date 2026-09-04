# GridLootMaster C7 콘텐츠 공백 메모 v1

최신 Source와 사용자 제공 ItemData 기준으로 확인한 내용.

## 확인된 현재 데이터

총기 5:
- Glock19
- MP5
- Mosin
- AK74M
- M4A1

탄약:
- 9x19mm
- 5.45x39mm
- 5.56x45mm

Magazine:
- Mag_M4 (5.56x45mm, 30발) 1개

## 결과

현재 Firearm 공격은 `EquippedMagazine`이 필수다.

따라서 M4A1 외 총기 4개는 현재 Content Data만으로 정상적인 Magazine/Reload 플레이 흐름을 완성할 수 없다.

특히 Mosin은 7.62x54mmR Ammo 행도 없다.

## 추가로 확인된 Source 위험

Magazine Attachment 장착 시 현재 UI 경로는 Magazine 타입 여부를 검사하지만
Weapon의 CompatibleAmmo와 Magazine의 CompatibleAmmo 일치 여부를 검사하지 않는다.

즉 현재는 M4 Magazine 같은 잘못된 구경 Magazine을 다른 Weapon에 장착할 수 있는 경로가 존재한다.

## 권장 최소 수정

최종 Combat 완료 전에:
- 9mm Magazine
- 5.45 Magazine
- 7.62x54R Magazine 또는 Mosin 전용 장전 규칙
- 7.62x54R Ammo
- Weapon/Magazine caliber compatibility validation

을 처리한다.

이 메모는 G1 Map 작업과 무관하므로 G1을 중단시키지 않는다.
