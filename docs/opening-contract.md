# 오프닝 복원·전환 계약

2026-10-23 사용자 승인 반영. 구현 기준은 규칙 단계 최종 커밋 `a7b8a56a0955d8dfc17974057f221b2bd8143dfa`다. 공개 헤더는 `src/opening/include/omok/opening/opening.hpp`, 구현은 `src/opening/opening.cpp`다. `omok_opening`은 state/rules를 사용하는 표준 C++20 정적 라이브러리이며 GUI·Windows·통신·시계·AI 탐색에 의존하지 않는다.

## 근거·데이터와 사용 조건

| 자료 | 확인한 형식·역할 | 사용·재배포 상태 |
|---|---|---|
| [Gomocup 일반 안내](https://gomocup.org/detail-information/) | 렌주 전문가 준비 오프닝·패스 금지·200수 정책 | 정책 근거. 이 페이지에서 사용할 렌주 데이터셋·재배포 허가를 확보한 것은 아님 |
| [GomocupJudge 고정 트리](https://github.com/Gomocup/GomocupJudge/tree/56cfd7dc1a0d1ec06d29f44a26931f375ad05991) | `server/server.py`의 `opening2pos`는 판 중심 상대 x,y 정수쌍을 compact 문자열로 변환. `client/utility.py`의 `str_to_pos`는 문자 x·1 기반 숫자 y를 순서대로 해독. `ai_match.__init__`은 1수 흑부터 교대해 복원 | 조사/비교 버전 `56cfd7dc1a0d1ec06d29f44a26931f375ad05991`. 트리에서 `server/opening/openings_20.txt`만 확인했으며 이것을 15×15 렌주 공식 데이터로 채택하지 않음. 렌주 데이터·재배포 조건 미확보 |
| [공식 프로토콜 SWAP2BOARD](https://plastovicka.github.io/protocl2en.htm) | 0/3/5돌 입력과 3돌·SWAP·4번째 수·추가 2돌·6번째 수 응답. 문서 이력상 2022-12-20 추가 | 의미만 구현. 명령 문자열·파이프는 미구현. 고정 Judge의 `doc/GomocupProtocol.md`도 확인 |
| [RenjuNet의 Gomoku Swap2 설명](https://www.renju.net/rule/11/) | 첫 제안자·두 번째 참가자의 선택 구조. 분류는 Gomoku | 렌주 결합 근거로 사용하지 않음 |
| [자체 테스트 배치](../tests/fixtures/prepared-self-authored.txt)·[출처 고지](../tests/fixtures/PREPARED-DATA.md) | 버전 1의 `h8i8g9`, 프로젝트 내 직접 작성 | 공식 대회 배치·전략이 아님. 외부 데이터에서 복사하지 않았으며 테스트에만 사용, GUI 패키지 install 대상 아님. 프로젝트 전체 공개 라이선스는 미선정 |

원본 심판·서버/유틸리티는 ignored build 캐시에서 조사·실행했으며 제품에 복사·번역·동봉하지 않는다. `check_forbid.py` 개별 GPL 표기 외에 저장소 전체·데이터의 통일 재배포 조건은 확정하지 않았다. 출처를 알 수 없는 외부 준비 배치를 자동 다운로드하거나 제품에 동봉하지 않는다. 참가 연도·리그·운영 심판도 미정이다. [추가 조사·검증 기록](verification-opening.md)에 파일 해시와 실행 범위를 남긴다.

## 준비 배치 입력·좌표

`load_compact(id, text, output)`는 **고정 심판 client compact position의 15×15 부분집합**을 읽는다. 전체 서버 오프닝 파일·PSQ·BOARD·SWAP2BOARD 파서가 아니다. 파일에서 한 줄을 읽은 호출자가 문자열을 전달할 수 있으며 파일 선택/다중 배치 목록/기보 저장은 별도 책임이다.

- `a`~`o`(대문자 허용)는 **열 x = 0~14**, 뒤 숫자 `1`~`15`는 **행 y + 1**이다. 상태로 옮길 때 `{row=y-1, column=letter-'a'}`로 변환한다. `a15→(14,0)`, `o1→(0,14)`, `b12→(11,1)`을 검사한다. 화면 픽셀 변환은 아니다.
- `h8i8g9`는 `(7,7)` 흑→`(7,8)` 백→`(8,6)` 흑이다. 원본 순서가 착수 순서이며 1수 흑부터 교대한다. 서버 중심 상대 입력 `0,0,1,0,-1,1`도 같은 문자열로 변환된다. 서버 raw CSV는 현재 로더의 입력 형식이 아니다.
- 내부 `PreparedData`는 출처/버전을 식별할 비어 있지 않은 id와 `Move{number, position, color}` 목록이다. id는 호출자의 표식이며 공식 인증 증명이 아니다. number는 1부터 연속, 색은 흑부터 교대다. typed API로 직접 만든 목록도 복원 시 재검증한다.
- compact 로더는 공백·구분자·주석·불완전 좌표·0/선행 0/세 자리 숫자·범위 밖·중복을 거부하고 200개까지만 허용한다. 완전 빈 문자열은 빈 준비 배치다(고정 서버의 빈 오프닝 fallback과 일치). 이 구문 검사는 원본 파서보다 엄격하다. 형식 파싱만 성공했다고 합법 렌주 데이터라는 뜻은 아니다.
- 실패하면 output 전체를 보존한다. 복원 함수는 별도로 중간 규칙 판정까지 수행한다. 부적합한 좌표가 패스가 되는 경로는 없다.

## 준비 배치의 원자적 복원

`Session::restore_prepared(data, black_owner)`는 **새 오프닝 초기화**다. 성공하면 이전 세션 전체를 새 세션으로 교체한다. 기존 대국에 추가하는 API가 아니며 실패하면 이전 보드·소유자·차례·결과·이력·mode/stage·id·이벤트를 모두 보존한다. 복원 시작 전에 유효한 흑 소유자 A/B를 명시하고 백은 다른 참가자로 유도한다.

각 수에 `rules::analyze_board`를 적용한다. 준비 배치마다 일반 대국용 `evaluate`를 호출하지 않는다. 순서·색·구조를 확인한 후 사본에 기록하고, **금수는 데이터 오류로 전체 거부**한다. 실제 참가자가 놓은 수가 아니므로 반칙패로 변환하지 않는다. 보드 판정은 rules에만 있고 opening이 패턴을 복제하지 않는다.

공식 RIF 9절과 기존 대회 수정 정책에 따라 승리한 최종 수는 종료 상태로 복원한다. 종료 수 뒤에 데이터가 더 있으면 `AfterTerminal`로 전체 거부한다. 총 200수의 마지막 착수는 승리를 먼저 판정하고 그렇지 않으면 Draw로 끝낸다. 201개 이상 목록은 거부한다. 준비 배치는 전체 이력·총 수순에 포함하며 제한을 초기화하지 않는다. 오프닝 길이가 홀수면 다음 백, 짝수면 다음 흑이다.

**비교 심판과의 차이:** 원본 `ai_match.__init__`은 준비 데이터를 신뢰해 배열에 직접 쓰며 금수·승리·중복·초기 길이를 검사하지 않는다. 그 생략을 제품의 허용 규정으로 복제하지 않았다. 위 거부/종료 처리는 공식 판정과 이번 사용자의 데이터 검증 요구에 따른 프로젝트의 가져오기 계약이다. 심판이 잘못된 준비 배치를 같은 방식으로 처리한다고 주장하지 않는다. 실제 공식 배치 확보 후 이 계약과 데이터의 적합성을 다시 검사해야 한다.

`Placement::placed_by`는 이번에 `optional<Participant>`로 확장했다. **nullopt는 외부에서 준비된 돌**이며 실제 참가자 미상이나 색 소유자를 뜻하지 않는다. 준비 배치에 A/B를 임의로 채우지 않는다. 직접 둔 수·Swap2 배치는 실제 주체를 기록한다. 색 배정으로 과거 nullopt나 A/B가 바뀌지 않는다. 일반 착수 제어는 입력 참가자를 확인하고 실제 actor를 반드시 공급한다. 상태 모듈은 계속 구조 저장만 담당한다.

## 빈 보드에서 시작하는 별도 렌주 대국

2026-10-24 후속 사용자 요청으로 `begin_direct_renju(black_owner)`를 추가했다. 기존 준비 배치 대신 `Mode::DirectRenju`, `Stage::Complete`, `RenjuReady`, ID `direct-renju-v1`, 빈 보드/이력·흑 차례·명시적 A/B 색 배정으로 초기화한다. Started/ColorsAssigned/Completed 이벤트를 기록하고 잘못된 참가자는 세션 전체를 보존하며 거부한다. 준비 돌이나 BatchPlaced 이벤트를 만들어내지 않는다.

기존 렌주 보드 판정·200수 정책을 재사용하는 로컬 시작 방식이다. 첫 수 위치를 제한하는 정식 오프닝 절차·전문가 준비 배치·Swap2의 대체 대회 규칙으로 소개하지 않는다. PreparedRenju 로더의 빈 문자열 처리와 match의 빈 준비 배치 거부는 유지한다. GUI는 처음부터를 초기 선택으로 제공하되 준비 배치 선택도 유지한다.

## 별도 Swap2 상태 기계 (기존 계약)

`begin_swap2()`는 새 `Mode::Swap2Protocol` 세션으로 초기화한다. 아래 A는 최초 제안자, B는 두 번째 참가자다. 선택의 Black/White는 엔진 자신/상대 표기가 아니다. `Move` 번호·색은 명시적으로 검증하고 배치는 묶음 전체를 원자적으로 적용한다.

| 현재 stage·입력 주체 | API·허용 동작 | 배치 번호·색 | 다음 stage·색 대응·차례 |
|---|---|---|---|
| FirstThree · A | `place_first_three(A, 3수)` | 1 흑·2 백·3 흑 | SecondChoice, B 입력. 색 소유 미배정·다음 돌 백 |
| SecondChoice · B | `choose(B, TakeBlack)` | 0수, SWAP 의미 | Complete. 흑 B·백 A, 4번째 백 A 차례 |
| SecondChoice · B | `choose(B, TakeWhite, 1수)` | 4 백, 선택과 함께 확정 | Complete. 흑 A·백 B, 5번째 흑 A 차례 |
| SecondChoice · B | `choose(B, OfferTwo, 2수)` | 4 백·5 흑 | FinalChoice, A 입력. 색 소유 미배정·다음 돌 백 |
| FinalChoice · A | `choose(A, TakeBlack)` | 0수, SWAP 의미 | Complete. 흑 A·백 B, 6번째 백 B 차례 |
| FinalChoice · A | `choose(A, TakeWhite, 1수)` | 6 백, 선택과 함께 확정 | Complete. 흑 B·백 A, 7번째 흑 B 차례 |

여기서는 프로토콜 의미에 맞춰 색 선택과 백의 4/6번째 수를 **한 응답 트랜잭션**으로 받는다. 색만 선택하고 나중에 백을 놓는 GUI 흐름을 확정한 것은 아니다. 전략·자동 색 선택은 없고 입력이 주어진 경우만 처리한다. 어느 초기 돌도 중앙으로 강제하지 않는다.

잘못된 enum·주체·stage·동작·개수·번호·색·좌표·중복은 거부한다. 특히 4번째 수가 정상이고 5번째 수가 잘못되어도 4번째 돌과 선택 이벤트까지 롤백한다. 색만 선택하는 경로에서는 보드와 이력이 완전히 같으며, 백 수가 함께 오는 경로도 기존 prefix는 그대로다. 완료 이후 추가 오프닝 응답은 거부한다. 준비 렌주 세션에 Swap2 응답을 적용할 수도 없다.

### Swap2 규칙 적용은 미해결 계약

SWAP2BOARD는 통신/전환 형식이며, 임의의 기본 렌주와 결합해도 된다는 근거가 아니다. Gomocup 2026의 Swap2는 무제한 Gomoku 대회 안내에 있고 현재 제품의 잠정 렌주 기본 대회와 분리된다. 이번 상태 기계는 **구조·차례·색만** 처리하며 렌주 금수·200수 정책을 자동 적용하지 않는다.

Complete는 Swap2 선택 절차 완료다. `handoff().continuation`은 `RuleContractRequired`, `can_enter_main_game()`은 false다. 다음 색과 소유자는 알려주지만 기존 렌주 `evaluate`로 바로 넘기지 않는다. 후속 단계에서 실제 대상 규칙·종료 정책을 근거와 함께 연결해야 한다. 이 미해결 사항은 현재 상태 전환 구현을 막지 않아 사용자에게 결합 규칙 승인을 요구하지 않았다. 기본값이나 추가 필수 게임 모드를 만들지 않았다.

## 본게임 제어·기보에 넘기는 계약

외부는 변경 가능한 GameState 참조를 받지 않는다. **Session 전체를 복사한 스냅샷**의 `game()`·`handoff()`·`events()`·`data_id()`를 함께 전달한다. 이 묶음에 보드·배치 이력·A/B 색 대응·다음 색/참가자·총 배치 수·대국 결과·오프닝 종류/단계가 들어 있다. 각 getter의 span은 사본이 아니므로 원 세션 변경을 가로질러 보관하지 않는다. 동시 쓰기 동기화는 이후 match가 맡는다.

| continuation | 제어 계층의 처리 |
|---|---|
| NotStarted | 초기화 필요, 착수 요청 금지 |
| OpeningInput | `input_actor`에 해당 오프닝 응답만 요청. 이 주체와 `game.next_participant()`를 혼동하지 않음 |
| RenjuReady | 본게임 진입 가능. 다음 참가자의 권한을 확인한 뒤 기존 `rules::evaluate`로 착수 판정 |
| Terminal | 승리/200수 등 이미 종료. 다음 입력 요청 금지 |
| RuleContractRequired | Swap2 절차만 완료. 실제 규칙 연결 전 일반 대국 요청 금지 |

match는 인계 스냅샷을 바탕으로 자체 가변 상태를 소유하고 일반 착수 권한·판정·배치·차례·종료의 일관된 적용을 담당한다. GUI나 AI를 상태 저장 API에 직접 연결하지 않는다. 오프닝이 일반 대국 루프나 반칙패·시계·자원 정책을 담당하지 않는다. 매니저 불일치 중단 정책도 유지한다.

이벤트에는 Started, BatchPlaced, ChoiceMade, ColorsAssigned, Completed가 있고 당시 주체·선택·흑 소유자·총 배치 수를 기록한다. mode와 준비 id는 세션 메타데이터, 돌 좌표·색·실제 주체는 game 이력에 있다. 준비 복원 이벤트의 actor는 없음이다. 향후 기보는 **메타데이터+이력+이벤트+결과**를 함께 저장해야 하며 이벤트만 저장하면 배치를 재현할 수 없다. 이번에는 메모리 내 기록만 구현했고 파일 저장·불러오기·복기는 구현하지 않았다.

새 초기화 함수는 이전 세션을 성공 시 교체한다. 단계별 응답은 성공 시에만 새 사본을 공개하며 실패 시 전체 보존한다. 최종 move assignment의 noexcept를 static_assert로 확인한다. 동적 메모리 할당 예외는 호출자에게 전달하며 원 세션은 유지하지만 할당 실패 주입 검증은 수행하지 않았다.

2026-10-24 후속: match가 원본 Session 전체와 별도 진행 상태를 보존한다. GUI에 자체 작성한 개발용 배치 다섯 가지를 제공하며 공식 데이터로 소개하지 않는다. 외부 데이터/테스트 fixture의 install 제외는 유지한다. [match 계약](match-contract.md)·[검증](verification-match.md)을 따른다. Swap2의 RuleContractRequired 차단은 그대로다.
