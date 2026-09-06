# GridLootMaster

> 그리드 인벤토리를 중심으로 제작한 Unreal Engine 5.7 기반 탈출형 루팅 게임  
> **Extraction Looter / Grid Inventory / C++**

---

## 프로젝트 소개

**GridLootMaster**는 제한된 인벤토리 공간에서 장비와 전리품을 관리하고,  
전장을 탐색하며 전투·수색·루팅을 진행한 뒤 탈출하여 자산을 축적하는  
**그리드 인벤토리 기반 탈출형 루팅 게임**입니다.

탈출형 게임의 핵심 루프인

`준비 → 레이드 → 탐색 → 전투 → 루팅 → 탈출 → 스태쉬 관리`

를 하나의 플레이 흐름으로 연결하는 것을 목표로 제작했습니다.

특히 단순한 슬롯형 인벤토리가 아닌, 아이템의 크기와 회전, 장비에 따른 수납 공간 변화,  
실패 시 데이터가 손실되지 않는 Transaction 기반 이동 구조를 구현하는 데 중점을 두었습니다.

---

## 개발 환경

| 항목 | 내용 |
| --- | --- |
| Engine | Unreal Engine 5.7 |
| Language | C++ |
| UI | UMG / Slate / WidgetTree |
| Data | DataTable / SaveGame |
| Testing | Unreal Automation Test |
| Version Control | Git / GitHub |
| Platform | Windows |
| Project Type | 개인 프로젝트 |

### 개발 원칙

- Blueprint Gameplay Logic 사용하지 않음
- WBP 기반 UI 사용하지 않음
- Gameplay 및 UI를 C++ 중심으로 구현
- 데이터 무결성과 실패 복구를 우선하여 시스템 설계

---

# 핵심 게임 루프

```text
OutRaid / Stash
        ↓
장비 · 탄약 준비
        ↓
START RAID
        ↓
맵 탐색
        ↓
Container / Enemy 탐색
        ↓
SEARCH / EXAMINE
        ↓
Loot
        ↓
Combat
        ↓
Enemy Corpse Loot
        ↓
Extraction
        ↓
Stash 저장
        ↓
아이템 판매
        ↓
Retirement Account 누적
        ↓
목표 금액 달성
        ↓
RETIRE / Ending