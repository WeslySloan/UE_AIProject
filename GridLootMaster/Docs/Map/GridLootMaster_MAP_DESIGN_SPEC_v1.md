# GridLootMaster 9×9 맵 디자인 명세서 v1.0
## Codex 구현 전달용 — 확정 권장안

기준:
- Unreal Engine 5.7
- C++ Only
- 현재 Source의 `UMapManagerComponent`, `FTileData`, A*, 4방향 벽, Extraction 기능을 유지하면서 맵 템플릿을 교체하는 것을 전제로 한다.
- 좌표: `(0,0)` ~ `(8,8)`
- 시작점: `(0,0)` 또는 `(8,8)`
- 외부 사례 검색은 사용하지 않았다. 아래 수치는 현재 GridLootMaster의 기획·기술 구조와 제공 Source를 기준으로 한 신규 확정 권장안이다.

---

# 1. 확정 권장안 요약

## 핵심 구조

```text
┌───────────┬───────────┬───────────┐
│ A         │ B         │ C         │
│ 북서      │ 북부      │ 북동      │
│ 보급창    │ 정비동    │ 무기고    │
├───────────┼───────────┼───────────┤
│ D         │ E         │ F         │
│ 서부      │ 중앙      │ 동부      │
│ 설비동    │ 통제구역  │ 설비동    │
├───────────┼───────────┼───────────┤
│ G         │ H         │ I         │
│ 남서      │ 남부      │ 남동      │
│ 의료/기술 │ 정비동    │ 보급창    │
└───────────┴───────────┴───────────┘
```

### 디자인 원칙

1. `(0,0)`과 `(8,8)` 시작의 공정성을 위해 **180도 회전 대칭**을 유지한다.
2. A/I는 진입·탈출 측 안전 구역이며 Loot가 낮다.
3. C/G는 메인 동선에서 벗어난 측면 고가치 구역이다.
4. E는 맵의 최고 위험·최고 가치 구역이다.
5. B/D/F/H는 우회와 연결을 담당하는 중간 위험 구역이다.
6. 모든 타일은 이동 가능하다. 장애물은 `타일 비활성화`가 아니라 **4방향 Edge Wall**로 표현한다.
7. 기본 맵에는 단 하나의 필수 타일/필수 통로도 두지 않는다.
8. 적 Spawn 가능 여부는 아래 표의 **정적 허용값**과 런타임 필터를 함께 사용한다.

---

# 2. 구역별 역할

| 구역 | 좌표 범위 | 구역명 | 게임플레이 역할 |
|---|---|---|---|
| A | X0-2 / Y0-2 | 북서 보급창 | 진입/안전 구역. 저가치 보급, 초반 동선 확보 |
| B | X3-5 / Y0-2 | 북부 정비동 | 중위험 연결 구역. 탄약·부품 중심 |
| C | X6-8 / Y0-2 | 북동 무기고 | 측면 고가치 POI. 무기·부착물 중심 |
| D | X0-2 / Y3-5 | 서부 설비동 | 중위험 우회 구역. 소모품·잡화 중심 |
| E | X3-5 / Y3-5 | 중앙 통제구역 | 최고위험/고가치 핵심 구역. 중앙 교차와 막다른 고가치 방 |
| F | X6-8 / Y3-5 | 동부 설비동 | 중위험 우회 구역. 소모품·잡화 중심 |
| G | X0-2 / Y6-8 | 남서 의료·기술창고 | 측면 고가치 POI. 의료·전자 전리품 중심 |
| H | X3-5 / Y6-8 | 남부 정비동 | 중위험 연결 구역. 탄약·부품 중심 |
| I | X6-8 / Y6-8 | 남동 보급창 | 진입/안전 구역. 저가치 보급, 초반 동선 확보 |

---

# 3. Loot 등급 정의

## Loot 위험도

| 등급 | 의미 |
|---|---|
| 1 | 시작/초기 안전 구역 |
| 2 | 낮은 위험 |
| 3 | 일반 교전 가능성이 있는 중간 위험 |
| 4 | 고가치 POI 및 주요 이동축 |
| 5 | 중앙 핵심/매복 위험이 높은 구역 |

## Loot 가치

| 등급 | 의미 |
|---|---|
| D | 기본 소모품/저가치 |
| C | 일반 Loot |
| B | 중가치 Loot |
| A | 고가치 Loot |
| S | 핵심 고가치 POI |

---

# 4. 9×9 전체 타일 구현 데이터

`Enemy Spawn`의 의미:
- `허용`: 정적 Spawn 후보가 될 수 있음. 런타임 거리/LOS/점유 검사를 추가로 통과해야 함.
- `금지`: 일반 랜덤 Spawn 후보에서 제외.
- `금지(매복 스크립트만)`: 일반 Spawn은 금지하지만 향후 고정 Ambusher 배치는 가능.
- `스크립트 전용`: Boss/Elite 등 명시적 스폰에만 사용.

| 좌표 | 구역 | 구역명 | 타일 타입 | 이동 | 주요 장애물 | Loot 위험 | Loot 가치 | 적 Spawn | 탈출 후보 |
|---|---|---|---|---|---|---|---|---|---|
| (0,0) | A | 북서 보급창 | Normal (선정 시 Extraction) | Yes | 없음 | 1-안전 | D-낮음 | 금지 | (8,8) 시작 |
| (1,0) | A | 북서 보급창 | Normal | Yes | 내부 엄폐벽 | 1-안전 | D-낮음 | 금지 | 아니오 |
| (2,0) | A | 북서 보급창 | Normal | Yes | 구역 경계벽 | 1-안전 | D-낮음 | 금지 | 아니오 |
| (3,0) | B | 북부 정비동 | Normal | Yes | 구역 경계벽 | 2-낮음 | C-보통 이하 | 허용 | 아니오 |
| (4,0) | B | 북부 정비동 | Normal | Yes | 내부 엄폐벽 | 2-낮음 | C-보통 이하 | 허용 | 아니오 |
| (5,0) | B | 북부 정비동 | Normal | Yes | 구역 경계벽 | 2-낮음 | C-보통 이하 | 허용 | 아니오 |
| (6,0) | C | 북동 무기고 | Normal | Yes | 구역 경계벽 | 3-중간 | B-보통 | 허용 | 아니오 |
| (7,0) | C | 북동 무기고 | Normal | Yes | 내부 엄폐벽 | 3-중간 | B-보통 | 허용 | 아니오 |
| (8,0) | C | 북동 무기고 | Normal | Yes | 없음 | 4-높음 | A-높음 | 허용 | 아니오 |
| (0,1) | A | 북서 보급창 | Normal (선정 시 Extraction) | Yes | 내부 엄폐벽 | 1-안전 | D-낮음 | 금지 | (8,8) 시작 |
| (1,1) | A | 북서 보급창 | Normal | Yes | 내부 엄폐벽 | 1-안전 | D-낮음 | 금지 | 아니오 |
| (2,1) | A | 북서 보급창 | Normal | Yes | 없음 | 2-낮음 | C-보통 이하 | 허용 | 아니오 |
| (3,1) | B | 북부 정비동 | Normal | Yes | 내부 엄폐벽 | 3-중간 | B-보통 | 허용 | 아니오 |
| (4,1) | B | 북부 정비동 | Normal | Yes | 내부 엄폐벽 | 3-중간 | B-보통 | 허용 | 아니오 |
| (5,1) | B | 북부 정비동 | Normal | Yes | 없음 | 3-중간 | B-보통 | 허용 | 아니오 |
| (6,1) | C | 북동 무기고 | Normal | Yes | 내부 엄폐벽 | 3-중간 | B-보통 | 허용 | 아니오 |
| (7,1) | C | 북동 무기고 | Normal | Yes | 내부 엄폐벽 | 4-높음 | A-높음 | 허용 | 아니오 |
| (8,1) | C | 북동 무기고 | Normal | Yes | 없음 | 3-중간 | B-보통 | 허용 | 아니오 |
| (0,2) | A | 북서 보급창 | Normal (선정 시 Extraction) | Yes | 구역 경계벽 | 1-안전 | D-낮음 | 금지 | (8,8) 시작 |
| (1,2) | A | 북서 보급창 | Normal | Yes | 없음 | 2-낮음 | C-보통 이하 | 허용 | 아니오 |
| (2,2) | A | 북서 보급창 | Normal | Yes | 구역 경계벽 | 2-낮음 | C-보통 이하 | 허용 | 아니오 |
| (3,2) | B | 북부 정비동 | Normal | Yes | 구역 경계벽 | 2-낮음 | C-보통 이하 | 허용 | 아니오 |
| (4,2) | B | 북부 정비동 | Normal | Yes | 없음 | 2-낮음 | C-보통 이하 | 허용 | 아니오 |
| (5,2) | B | 북부 정비동 | Normal | Yes | 구역 경계벽 | 2-낮음 | C-보통 이하 | 허용 | 아니오 |
| (6,2) | C | 북동 무기고 | Normal | Yes | 구역 경계벽 | 3-중간 | B-보통 | 허용 | 아니오 |
| (7,2) | C | 북동 무기고 | Normal | Yes | 없음 | 3-중간 | B-보통 | 허용 | 아니오 |
| (8,2) | C | 북동 무기고 | Normal | Yes | 구역 경계벽 | 3-중간 | B-보통 | 허용 | 아니오 |
| (0,3) | D | 서부 설비동 | Normal | Yes | 구역 경계벽 | 2-낮음 | C-보통 이하 | 허용 | 아니오 |
| (1,3) | D | 서부 설비동 | Normal | Yes | 내부 엄폐벽 | 3-중간 | B-보통 | 허용 | 아니오 |
| (2,3) | D | 서부 설비동 | Normal | Yes | 구역 경계벽 | 2-낮음 | C-보통 이하 | 허용 | 아니오 |
| (3,3) | E | 중앙 통제구역 | DeadEnd | Yes | 막다른 고가치 방 / 내부 엄폐벽 | 5-매우 높음 | S-최고 | 금지(매복 스크립트만) | 아니오 |
| (4,3) | E | 중앙 통제구역 | Normal | Yes | 내부 엄폐벽 | 4-높음 | A-높음 | 허용 | 아니오 |
| (5,3) | E | 중앙 통제구역 | DeadEnd | Yes | 막다른 고가치 방 / 내부 엄폐벽 | 5-매우 높음 | S-최고 | 금지(매복 스크립트만) | 아니오 |
| (6,3) | F | 동부 설비동 | Normal | Yes | 구역 경계벽 | 2-낮음 | C-보통 이하 | 허용 | 아니오 |
| (7,3) | F | 동부 설비동 | Normal | Yes | 없음 | 3-중간 | B-보통 | 허용 | 아니오 |
| (8,3) | F | 동부 설비동 | Normal | Yes | 구역 경계벽 | 2-낮음 | C-보통 이하 | 허용 | 아니오 |
| (0,4) | D | 서부 설비동 | Normal | Yes | 내부 엄폐벽 | 2-낮음 | C-보통 이하 | 허용 | 아니오 |
| (1,4) | D | 서부 설비동 | Normal | Yes | 내부 엄폐벽 | 3-중간 | B-보통 | 허용 | 아니오 |
| (2,4) | D | 서부 설비동 | Normal | Yes | 없음 | 2-낮음 | C-보통 이하 | 허용 | 아니오 |
| (3,4) | E | 중앙 통제구역 | Normal | Yes | 내부 엄폐벽 | 4-높음 | A-높음 | 허용 | 아니오 |
| (4,4) | E | 중앙 통제구역 | Normal | Yes | 내부 엄폐벽 | 5-매우 높음 | S-최고 | 스크립트 전용(보스/엘리트) | 아니오 |
| (5,4) | E | 중앙 통제구역 | Normal | Yes | 내부 엄폐벽 | 4-높음 | A-높음 | 허용 | 아니오 |
| (6,4) | F | 동부 설비동 | Normal | Yes | 없음 | 2-낮음 | C-보통 이하 | 허용 | 아니오 |
| (7,4) | F | 동부 설비동 | Normal | Yes | 내부 엄폐벽 | 3-중간 | B-보통 | 허용 | 아니오 |
| (8,4) | F | 동부 설비동 | Normal | Yes | 내부 엄폐벽 | 2-낮음 | C-보통 이하 | 허용 | 아니오 |
| (0,5) | D | 서부 설비동 | Normal | Yes | 구역 경계벽 | 2-낮음 | C-보통 이하 | 허용 | 아니오 |
| (1,5) | D | 서부 설비동 | Normal | Yes | 없음 | 3-중간 | B-보통 | 허용 | 아니오 |
| (2,5) | D | 서부 설비동 | Normal | Yes | 구역 경계벽 | 2-낮음 | C-보통 이하 | 허용 | 아니오 |
| (3,5) | E | 중앙 통제구역 | DeadEnd | Yes | 막다른 고가치 방 / 내부 엄폐벽 | 5-매우 높음 | S-최고 | 금지(매복 스크립트만) | 아니오 |
| (4,5) | E | 중앙 통제구역 | Normal | Yes | 내부 엄폐벽 | 4-높음 | A-높음 | 허용 | 아니오 |
| (5,5) | E | 중앙 통제구역 | DeadEnd | Yes | 막다른 고가치 방 / 내부 엄폐벽 | 5-매우 높음 | S-최고 | 금지(매복 스크립트만) | 아니오 |
| (6,5) | F | 동부 설비동 | Normal | Yes | 구역 경계벽 | 2-낮음 | C-보통 이하 | 허용 | 아니오 |
| (7,5) | F | 동부 설비동 | Normal | Yes | 내부 엄폐벽 | 3-중간 | B-보통 | 허용 | 아니오 |
| (8,5) | F | 동부 설비동 | Normal | Yes | 구역 경계벽 | 2-낮음 | C-보통 이하 | 허용 | 아니오 |
| (0,6) | G | 남서 의료·기술창고 | Normal | Yes | 구역 경계벽 | 3-중간 | B-보통 | 허용 | 아니오 |
| (1,6) | G | 남서 의료·기술창고 | Normal | Yes | 없음 | 3-중간 | B-보통 | 허용 | 아니오 |
| (2,6) | G | 남서 의료·기술창고 | Normal | Yes | 구역 경계벽 | 3-중간 | B-보통 | 허용 | 아니오 |
| (3,6) | H | 남부 정비동 | Normal | Yes | 구역 경계벽 | 2-낮음 | C-보통 이하 | 허용 | 아니오 |
| (4,6) | H | 남부 정비동 | Normal | Yes | 없음 | 2-낮음 | C-보통 이하 | 허용 | 아니오 |
| (5,6) | H | 남부 정비동 | Normal | Yes | 구역 경계벽 | 2-낮음 | C-보통 이하 | 허용 | 아니오 |
| (6,6) | I | 남동 보급창 | Normal | Yes | 구역 경계벽 | 2-낮음 | C-보통 이하 | 허용 | 아니오 |
| (7,6) | I | 남동 보급창 | Normal | Yes | 없음 | 2-낮음 | C-보통 이하 | 허용 | 아니오 |
| (8,6) | I | 남동 보급창 | Normal (선정 시 Extraction) | Yes | 구역 경계벽 | 1-안전 | D-낮음 | 금지 | (0,0) 시작 |
| (0,7) | G | 남서 의료·기술창고 | Normal | Yes | 없음 | 3-중간 | B-보통 | 허용 | 아니오 |
| (1,7) | G | 남서 의료·기술창고 | Normal | Yes | 내부 엄폐벽 | 4-높음 | A-높음 | 허용 | 아니오 |
| (2,7) | G | 남서 의료·기술창고 | Normal | Yes | 내부 엄폐벽 | 3-중간 | B-보통 | 허용 | 아니오 |
| (3,7) | H | 남부 정비동 | Normal | Yes | 없음 | 3-중간 | B-보통 | 허용 | 아니오 |
| (4,7) | H | 남부 정비동 | Normal | Yes | 내부 엄폐벽 | 3-중간 | B-보통 | 허용 | 아니오 |
| (5,7) | H | 남부 정비동 | Normal | Yes | 내부 엄폐벽 | 3-중간 | B-보통 | 허용 | 아니오 |
| (6,7) | I | 남동 보급창 | Normal | Yes | 없음 | 2-낮음 | C-보통 이하 | 허용 | 아니오 |
| (7,7) | I | 남동 보급창 | Normal | Yes | 내부 엄폐벽 | 1-안전 | D-낮음 | 금지 | 아니오 |
| (8,7) | I | 남동 보급창 | Normal (선정 시 Extraction) | Yes | 내부 엄폐벽 | 1-안전 | D-낮음 | 금지 | (0,0) 시작 |
| (0,8) | G | 남서 의료·기술창고 | Normal | Yes | 없음 | 4-높음 | A-높음 | 허용 | 아니오 |
| (1,8) | G | 남서 의료·기술창고 | Normal | Yes | 내부 엄폐벽 | 3-중간 | B-보통 | 허용 | 아니오 |
| (2,8) | G | 남서 의료·기술창고 | Normal | Yes | 구역 경계벽 | 3-중간 | B-보통 | 허용 | 아니오 |
| (3,8) | H | 남부 정비동 | Normal | Yes | 구역 경계벽 | 2-낮음 | C-보통 이하 | 허용 | 아니오 |
| (4,8) | H | 남부 정비동 | Normal | Yes | 내부 엄폐벽 | 2-낮음 | C-보통 이하 | 허용 | 아니오 |
| (5,8) | H | 남부 정비동 | Normal | Yes | 구역 경계벽 | 2-낮음 | C-보통 이하 | 허용 | 아니오 |
| (6,8) | I | 남동 보급창 | Normal | Yes | 구역 경계벽 | 1-안전 | D-낮음 | 금지 | 아니오 |
| (7,8) | I | 남동 보급창 | Normal | Yes | 내부 엄폐벽 | 1-안전 | D-낮음 | 금지 | 아니오 |
| (8,8) | I | 남동 보급창 | Normal (선정 시 Extraction) | Yes | 없음 | 1-안전 | D-낮음 | 금지 | (0,0) 시작 |

---

# 5. 북·남·동·서 벽 정보

표기:
- `Open`: 이동 가능
- `Wall`: 내부 벽/구역 벽으로 차단
- `Boundary`: 맵 외곽. Runtime에서는 `false`와 동일하게 이동 불가

| 좌표 | North | South | East | West |
|---|---|---|---|---|
| (0,0) | Boundary | Open | Open | Boundary |
| (1,0) | Boundary | Wall | Open | Open |
| (2,0) | Boundary | Open | Wall | Open |
| (3,0) | Boundary | Open | Open | Wall |
| (4,0) | Boundary | Wall | Open | Open |
| (5,0) | Boundary | Open | Wall | Open |
| (6,0) | Boundary | Open | Open | Wall |
| (7,0) | Boundary | Wall | Open | Open |
| (8,0) | Boundary | Open | Boundary | Open |
| (0,1) | Open | Open | Wall | Boundary |
| (1,1) | Wall | Open | Open | Wall |
| (2,1) | Open | Open | Open | Open |
| (3,1) | Open | Open | Wall | Open |
| (4,1) | Wall | Open | Open | Wall |
| (5,1) | Open | Open | Open | Open |
| (6,1) | Open | Open | Wall | Open |
| (7,1) | Wall | Open | Open | Wall |
| (8,1) | Open | Open | Boundary | Open |
| (0,2) | Open | Wall | Open | Boundary |
| (1,2) | Open | Open | Open | Open |
| (2,2) | Open | Wall | Wall | Open |
| (3,2) | Open | Open | Open | Wall |
| (4,2) | Open | Open | Open | Open |
| (5,2) | Open | Open | Wall | Open |
| (6,2) | Open | Wall | Open | Wall |
| (7,2) | Open | Open | Open | Open |
| (8,2) | Open | Wall | Boundary | Open |
| (0,3) | Wall | Open | Open | Boundary |
| (1,3) | Open | Wall | Open | Open |
| (2,3) | Wall | Open | Open | Open |
| (3,3) | Open | Open | Wall | Open |
| (4,3) | Open | Open | Open | Wall |
| (5,3) | Open | Wall | Open | Open |
| (6,3) | Wall | Open | Open | Open |
| (7,3) | Open | Open | Open | Open |
| (8,3) | Wall | Open | Boundary | Open |
| (0,4) | Open | Open | Wall | Boundary |
| (1,4) | Wall | Open | Open | Wall |
| (2,4) | Open | Open | Open | Open |
| (3,4) | Open | Wall | Wall | Open |
| (4,4) | Open | Open | Wall | Wall |
| (5,4) | Wall | Open | Open | Wall |
| (6,4) | Open | Open | Open | Open |
| (7,4) | Open | Wall | Wall | Open |
| (8,4) | Open | Open | Boundary | Wall |
| (0,5) | Open | Wall | Open | Boundary |
| (1,5) | Open | Open | Open | Open |
| (2,5) | Open | Wall | Open | Open |
| (3,5) | Wall | Open | Open | Open |
| (4,5) | Open | Open | Wall | Open |
| (5,5) | Open | Open | Open | Wall |
| (6,5) | Open | Wall | Open | Open |
| (7,5) | Wall | Open | Open | Open |
| (8,5) | Open | Wall | Boundary | Open |
| (0,6) | Wall | Open | Open | Boundary |
| (1,6) | Open | Open | Open | Open |
| (2,6) | Wall | Open | Wall | Open |
| (3,6) | Open | Open | Open | Wall |
| (4,6) | Open | Open | Open | Open |
| (5,6) | Open | Open | Wall | Open |
| (6,6) | Wall | Open | Open | Wall |
| (7,6) | Open | Open | Open | Open |
| (8,6) | Wall | Open | Boundary | Open |
| (0,7) | Open | Open | Open | Boundary |
| (1,7) | Open | Wall | Wall | Open |
| (2,7) | Open | Open | Open | Wall |
| (3,7) | Open | Open | Open | Open |
| (4,7) | Open | Wall | Wall | Open |
| (5,7) | Open | Open | Open | Wall |
| (6,7) | Open | Open | Open | Open |
| (7,7) | Open | Wall | Wall | Open |
| (8,7) | Open | Open | Boundary | Wall |
| (0,8) | Open | Boundary | Open | Boundary |
| (1,8) | Wall | Boundary | Open | Open |
| (2,8) | Open | Boundary | Wall | Open |
| (3,8) | Open | Boundary | Open | Wall |
| (4,8) | Wall | Boundary | Open | Open |
| (5,8) | Open | Boundary | Wall | Open |
| (6,8) | Open | Boundary | Open | Wall |
| (7,8) | Wall | Boundary | Open | Open |
| (8,8) | Open | Boundary | Boundary | Open |

---

# 6. 구현용 Closed Edge 목록

현재 Source의 `InitializeMap()`에 이미 존재하는 `CloseEdge(X1,Y1,X2,Y2)` 방식과 가장 잘 맞는 데이터다.

한 Edge는 한 번만 기록하며 구현 시 양쪽 Tile의 Open Flag를 동시에 닫아야 한다.

| From | To | 벽 종류 |
|---|---|---|
| (1,0) | (1,1) | InternalCover |
| (2,0) | (3,0) | ZoneBoundary |
| (4,0) | (4,1) | InternalCover |
| (5,0) | (6,0) | ZoneBoundary |
| (7,0) | (7,1) | InternalCover |
| (0,1) | (1,1) | InternalCover |
| (3,1) | (4,1) | InternalCover |
| (6,1) | (7,1) | InternalCover |
| (0,2) | (0,3) | ZoneBoundary |
| (2,2) | (3,2) | ZoneBoundary |
| (2,2) | (2,3) | ZoneBoundary |
| (5,2) | (6,2) | ZoneBoundary |
| (6,2) | (6,3) | ZoneBoundary |
| (8,2) | (8,3) | ZoneBoundary |
| (1,3) | (1,4) | InternalCover |
| (3,3) | (4,3) | InternalCover |
| (5,3) | (5,4) | InternalCover |
| (0,4) | (1,4) | InternalCover |
| (3,4) | (4,4) | InternalCover |
| (3,4) | (3,5) | InternalCover |
| (4,4) | (5,4) | InternalCover |
| (7,4) | (8,4) | InternalCover |
| (7,4) | (7,5) | InternalCover |
| (0,5) | (0,6) | ZoneBoundary |
| (2,5) | (2,6) | ZoneBoundary |
| (4,5) | (5,5) | InternalCover |
| (6,5) | (6,6) | ZoneBoundary |
| (8,5) | (8,6) | ZoneBoundary |
| (2,6) | (3,6) | ZoneBoundary |
| (5,6) | (6,6) | ZoneBoundary |
| (1,7) | (2,7) | InternalCover |
| (1,7) | (1,8) | InternalCover |
| (4,7) | (5,7) | InternalCover |
| (4,7) | (4,8) | InternalCover |
| (7,7) | (8,7) | InternalCover |
| (7,7) | (7,8) | InternalCover |
| (2,8) | (3,8) | ZoneBoundary |
| (5,8) | (6,8) | ZoneBoundary |

---

# 7. 시작점별 탈출 후보 확정안

## 시작 `(0,0)`

탈출 후보는 **동쪽 외곽의 남쪽 3칸**으로 한정한다.

```text
(8,6)
(8,7)
(8,8)
```

## 시작 `(8,8)`

180도 회전 대칭:

```text
(0,0)
(0,1)
(0,2)
```

### 거리 검증

| 시작 | 탈출 후보 | 맨해튼 거리 | 본 템플릿 최단 경로 |
|---|---|---|---|
| (0,0) | (8,6) | 14 | 14 |
| (0,0) | (8,7) | 15 | 15 |
| (0,0) | (8,8) | 16 | 16 |
| (8,8) | (0,0) | 16 | 16 |
| (8,8) | (0,1) | 15 | 15 |
| (8,8) | (0,2) | 14 | 14 |

확정 최소값:

```text
Minimum Manhattan Distance = 14
Minimum A* Path Length      = 14
```

현재 템플릿의 후보들은 모두 최단 경로 14~16 사이이므로
탈출 RNG 때문에 레이드 이동량이 지나치게 벌어지는 문제를 제한한다.

---

# 8. 탈출 선택 규칙

Raid Start:

```text
Player Start 확정
↓
해당 Start의 3개 Extraction Candidate 로드
↓
각 Candidate A* 재검증
↓
거리 >= 14 검사
↓
유효 후보 중 Uniform Random 1개 선택
↓
ExtractionPoints에 1개 등록
```

`ExtractionPointCount = 1`을 유지한다.

동적 벽이나 이후 맵 변형이 추가되더라도
A* 검사를 제거하지 않는다.

---

# 9. 탈출 정보 공개 규칙

현재 Source는 Extraction Tile을 처음부터 `EXIT`로 표시한다.

**신규 권장 기획에서는 정확한 Exit Tile을 Raid 시작 직후부터 보여주지 않는다.**

## `(0,0)` 시작

초기에는:

```text
탈출 방향: 동남쪽 외곽
```

정도만 표시.

정확한 Tile은 다음 중 먼저 충족할 때 공개:

1. Player가 Zone I에 진입
2. 선택된 Exit와 Chebyshev Distance <= 2

## `(8,8)` 시작

동일 규칙을 180도 대칭 적용:

```text
탈출 방향: 북서쪽 외곽
```

Zone A 진입 또는 Exit 2 Tile 이내에서 정확한 위치 공개.

### 이유

- 처음부터 최단 경로만 따라가는 플레이 방지
- Exit 후보가 3개이므로 순수 운빨은 제한
- 반대편 구역에 도착하면 정확한 위치를 알려 불필요한 마지막 탐색 스트레스 방지

이 항목은 **현재 Source와 다른 신규 UI/게임플레이 규칙**이므로 별도 구현 작업이다.

---

# 10. 적 Spawn 규칙

## 10.1 정적 Spawn 금지

일반 Enemy는 다음 타일에서 Spawn하지 않는다.

### 두 잠재 Start 주변

`(0,0)`에서 맨해튼 거리 <= 2:

```text
(0,0) (1,0) (2,0)
(0,1) (1,1)
(0,2)
```

`(8,8)`에서 맨해튼 거리 <= 2:

```text
(8,8) (7,8) (6,8)
(8,7) (7,7)
(8,6)
```

이는 반대 시작에서 Extraction 후보 일부와도 겹친다.

### 중앙 DeadEnd

```text
(3,3)
(5,3)
(3,5)
(5,5)
```

일반 Random Spawn 금지.

향후 `Ambusher`의 Scripted Placement만 허용.

### 중앙 Boss Anchor

```text
(4,4)
```

일반 SCAV Spawn 금지.
Boss/Elite Script Spawn 전용.

---

## 10.2 런타임 Spawn 필터

정적 `EnemySpawn=허용`이어도 다음 중 하나면 Spawn 금지:

```text
Player 현재 Tile
이미 Enemy가 점유한 Tile
선택된 Extraction Tile
Player와 Manhattan Distance < 3
A*로 Player 또는 Patrol 영역과 연결되지 않는 Tile
현재 Player가 명확하게 보고 있는 Tile
```

권장:
- LOS 시스템 구현 전에는 `Player Distance <= 2`를 보수적인 임시 시야 금지 영역으로 사용.
- LOS가 구현되면 실제 `Player Visible Tiles`로 교체.

---

# 11. 초기 안전 구역

선택된 Start 기준:

```text
Manhattan Distance <= 2
```

를 초기 안전 구역으로 사용한다.

권장 초기 안전 시간:

```text
2 World Ticks
```

첫 2 World Tick 동안:
- 이 영역에 Enemy Spawn 금지
- 이미 존재하는 Enemy도 이 영역으로 진입 금지

2 Tick 이후:
- Spawn 금지는 계속 유지
- Enemy AI의 이동 진입은 허용

즉 적이 안전 구역 안에서 갑자기 생성되지는 않지만,
시간을 너무 끌면 적이 진입할 수 있다.

---

# 12. 연결성 검증 규칙

구현 후 Automation Test에서 다음을 모두 검증한다.

## 필수

- MapGrid.Num() == 81
- 모든 Coordinate는 `(0,0)~(8,8)`
- 모든 Tile은 `(0,0)`에서 도달 가능
- 모든 Tile은 `(8,8)`에서 도달 가능
- 각 Wall은 양방향 대칭
  - A.East == B.West
  - A.North == B.South
- 각 Extraction Candidate까지 A* 존재
- Extraction Candidate Manhattan >= 14
- Extraction Candidate A* path length >= 14

## 본 확정 템플릿 추가 기준

이 템플릿은 설계상 다음 조건도 만족한다.

```text
Reachable Tiles          = 81 / 81
Articulation Tile Count  = 0
Bridge Edge Count        = 0
```

즉 하나의 특정 타일 또는 하나의 특정 통로만이
맵 양쪽을 연결하는 구조가 아니다.

Codex Automation Test에서도 가능하면
이 속성을 검증하는 Helper를 추가한다.

---

# 13. 현재 MapManager에 반영하기 쉬운 구조

## 유지

현재 Runtime:

```text
FTileData
- Coordinate
- Zone
- TileType
- bOpenNorth
- bOpenSouth
- bOpenEast
- bOpenWest
- bIsExplored
```

는 유지한다.

## 권장 추가 Metadata

Gameplay 설계가 실제 구현 단계에 들어갈 때 다음 필드를 별도 Template/Metadata로 두는 것을 권장:

```text
LootRiskTier
LootValueTier
bEnemySpawnAllowed
bStartSafeCandidate
ExtractionCandidateMask
CoverValue (전투 시스템 단계에서)
```

`ETileType`에 LootRisk를 억지로 넣지 않는다.

## 벽 Authoring

벽은 81개 Tile의 4 Bool을 손으로 각각 입력하기보다:

```text
ClosedEdge
From
To
```

목록으로 Authoring한다.

현재 Source의 `CloseEdge()`처럼
하나의 Edge 입력이 두 타일의 방향 Bool을 동시에 수정하게 한다.

이 방식은:

```text
(2,0).East = Wall
(3,0).West = Open
```

같은 비대칭 데이터 오류를 막는다.

---

# 14. 구현 파일 권장

현재 구조를 기준으로 최소 영향:

```text
Map/MapManagerComponent.cpp
Map/MapManagerComponent.h
Map/MapData.h
CombatAutomationTest.cpp 또는 신규 MapAutomationTest.cpp
UI/MinimapWidget.cpp/.h        // Exit 공개 규칙을 구현할 때만
UI/MinimapTileWidget.cpp/.h    // Exit 공개 규칙을 구현할 때만
```

맵 템플릿 변경을 이유로
Inventory/Equipment/Stash 코드를 수정하지 않는다.

---

# 15. Codex 구현 순서

## M0 — Baseline

- 최신 Build
- 68 Automation Test Green 확인

## M1 — 맵 Template 교체

- 기존 테스트용 Wall 배치 제거
- 본 문서 `Closed Edge` 적용
- Zone/TileType 반영

## M2 — Map Validation Test

- 81 Tile
- Wall Symmetry
- Full Connectivity
- No Bridge / No Articulation
- Candidate Path 확인

## M3 — Extraction Candidate 규칙 교체

현재의 `동쪽/서쪽 전체 Edge에서 Random` 대신
Start별 위 3개 후보를 사용.

A* Runtime 검증은 유지.

## M4 — Spawn Metadata

EnemyManager C2/C3 단계에서
본 문서의 Spawn Eligibility를 반영.

## M5 — Exit Reveal

전투/Enemy World 기반이 안정된 뒤
정확한 Exit 공개 지연 규칙을 UI에 적용.

---

# 16. 구현 전에 반드시 결정할 항목

현재 명세로 맵 Geometry 자체는 구현 가능하다.

아래는 후속 시스템과 결합 전에 사용자 결정이 필요한 항목이다.

1. **Exit 정확한 위치를 처음부터 보여줄 것인가?**
   - 권장: 반대 구역 방향만 표시 → 접근 시 정확한 위치 공개.

2. **C/G 고가치 POI의 실제 Loot Table은 무엇인가?**
   - 현재 문서는 가치 등급만 확정.

3. **E 중앙 DeadEnd 4칸에 고정 Container를 둘 것인가?**
   - 권장: 고정 고가치 Container 또는 높은 Spawn Table.

4. **(4,4)를 Boss 고정 Anchor로 사용할 것인가?**
   - 권장: Boss Profile이 도입되면 사용.

5. **Zone별 시각 테마/미니맵 명칭을 그대로 노출할 것인가?**
   - 시스템 구현에는 영향 없음.

---

# 17. Codex 전달용 핵심 성공 기준

```text
[ ] 좌표는 0~8만 사용한다.
[ ] 81개 모든 Tile이 연결된다.
[ ] 벽은 항상 양방향 대칭이다.
[ ] 시작점은 (0,0) 또는 (8,8)이다.
[ ] Start(0,0) Exit 후보 = (8,6),(8,7),(8,8)
[ ] Start(8,8) Exit 후보 = (0,0),(0,1),(0,2)
[ ] Exit Manhattan 최소 14
[ ] Exit A* Path 최소 14
[ ] 일반 Enemy는 두 시작 Corner 주변에 Spawn하지 않는다.
[ ] 중앙 DeadEnd에는 Random Spawn하지 않는다.
[ ] (4,4)는 Scripted Boss/Elite 전용이다.
[ ] 현재 보이는 Tile에는 Runtime Spawn하지 않는다.
[ ] 한 개의 필수 chokepoint에 맵 전체가 의존하지 않는다.
[ ] BP/WBP를 추가하지 않는다.
```
