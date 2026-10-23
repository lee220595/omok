# 렌주 판정 사례 명세

2026-10-23. 정확한 초기 배치는 버전 관리되는 [renju.tsv](../tests/fixtures/renju.tsv)의 각 ID 행을 기준으로 한다. 행의 필드는 `id|색|행|열|예상 Verdict|예상 Reason|흑 좌표 목록|백 좌표 목록`이다. 목록은 `행,열;행,열`이며 생략한 칸은 모두 빈칸이다. 모든 좌표는 0부터 시작한다. 각 행이 **초기 배치·후보·색·기대값**을 직접 포함하므로 임의 대국 수순으로 바꾸어 읽지 않는다. 합법 대국에서 도달 가능하다는 주장은 하지 않는다.

아래 이유와 근거를 TSV와 함께 각 사례의 명세로 사용한다. 근거 약호 R3/R9/R10은 [RIF 규칙](https://www.renju.net/rifrules/) 3/9/10.1이며 G는 [Gomocup 수정 정책](https://gomocup.org/detail-information/), J는 [고정 심판](https://github.com/Gomocup/GomocupJudge/tree/56cfd7dc1a0d1ec06d29f44a26931f375ad05991)의 실제 코드다. S는 상태 좌표·구조 계약이다. 기대값은 공식 정의에서 도출하고 별도 실제 심판과 대조했다. 각 TSV 사례는 8개 회전·대칭 변환과 원본 보드 보존을 검사한다. 가로·대각선 기초 사례를 각각 두어 세로·양 대각선·반대 가장자리도 검증한다.

| ID | 예상 판정·이유 | 근거 |
|---|---|---|
| empty | Legal/None: 위협 없는 빈 보드 착수 | R3 |
| outside | Invalid/OutOfBounds: (-1,7)은 판 밖 | S |
| occupied | Invalid/Occupied: 이미 흑이 있는 (7,7) | S |
| black_five | Win/ExactFive: 흑이 (7,3)~(7,7) 정확한 5 완성 | R9.1~2 |
| white_five | Win/WhiteFiveOrMore: 백 5 완성 | R9.1 |
| edge_five | Win/ExactFive: (0,0)에서 가장자리의 5 완성 | R9.1 |
| diagonal_five | Win/ExactFive: 우하향 대각선 5, 회전·대칭으로 우상향도 검사 | R9.1 |
| white_diagonal_five | Win/WhiteFiveOrMore: 백의 대각선 5 | R9.1 |
| edge_diagonal_five | Win/ExactFive: 모서리에서 대각선 5 완성 | R9.1 |
| diagonal_overline | Forbidden/Overline: 대각선 흑 6 | R9.2a |
| black_overline | Forbidden/Overline: 끝에 붙여 흑 6 완성 | R9.2a |
| black_split_overline | Forbidden/Overline: 가운데를 메워 흑 6 완성 | R9.2a |
| white_overline | Win/WhiteFiveOrMore: 백 6 허용·승리 | R9.1 |
| white_long_overline | Win/WhiteFiveOrMore: 백 7도 승리 | R9.1 |
| double_four | Forbidden/DoubleFour: 가로·세로 각 하나의 사 | R3·R9.2b |
| same_axis_fours | Forbidden/DoubleFour: `X.XXX.X`의 서로 다른 네 돌 집합 두 개 | R3·R9.2b |
| same_axis_broken_fours | Forbidden/DoubleFour: `XXX.X.XXX` 중앙 돌에서 만나는 두 사 | R3·R9.2b |
| open_four_once | Legal/None: 열린 사의 양끝을 두 사로 세지 않음 | R3 |
| double_three | Forbidden/DoubleThree: 가로·세로 각각 합법 열린 사로 확장 가능 | R3·R9.2c~3 |
| broken_double_three | Forbidden/DoubleThree: 끊긴 삼도 두 방향에서 확장 가능 | R3·R9.3 |
| open_three_once | Legal/None: 두 확장점은 같은 세 돌 집합 하나 | R3 |
| blocked_false_three | Legal/None: 가로 양끝 백으로 열린 사를 만들 수 없음 | R3·R9.3 |
| edge_false_three | Legal/None: 세로가 판 끝에 닿아 열린 사 불가 | R3·R9.3 |
| overline_false_three | Legal/None: 가로의 유일한 확장 (7,9)이 장목 | R9.3a |
| double_four_false_three | Legal/None: 가로 확장 (7,9)이 사사 | R9.3a |
| five_extension_false_three | Legal/None: 가로 확장 (7,9)이 동시에 세로 5목. 삼 정의에서 제외 | R3 THREE |
| recursive_false_three | Legal/None: 확장 (7,9)이 세로·우상향 대각의 진짜 삼삼이므로 가로 삼 제외 | R9.3b |
| recursive_exception | Forbidden/DoubleThree: (7,9)의 세로 확장 중 (9,9)은 백, (5,9)은 장목. 그 세로 삼은 가짜여서 (7,9)은 허용되고 원래 가로 삼은 진짜 | R9.3a~b |
| five_over_overline | Win/ExactFive: 가로 5와 세로 6 동시, 5 우선 | R9.2·J foulr A5 |
| five_over_double_four | Win/ExactFive: 5와 두 다른 사 동시, 5 우선 | R9.2·J |
| five_over_double_three | Win/ExactFive: 5와 두 삼 동시, 5 우선 | R9.2·J |
| white_double_three | Legal/None: 백 삼삼은 금수 아님 | R9.2 |
| white_double_four | Legal/None: 백 사사는 금수 아님 | R9.2 |
| blocked_four_not_four | Legal/None: 가로 네 돌의 양끝 백, 사로 세지 않음 | R3 |
| overline_completion_not_four | Legal/None: 가로 채움은 장목이어서 사가 아님. 세로 사 하나만 존재 | R3 |

## 구조·상태·수순 추가 사례

아래는 `tests/rules_tests.cpp`의 같은 이름 함수에 초기 배치 생성식과 후보를 명시했다. TSV가 표현하지 않는 상태 문맥을 검증한다.

| CTest | 초기 배치/수순·후보·색 | 예상 판정·이유·근거 |
|---|---|---|
| structural | 빈 보드의 네 모서리 흑; 행/열 15·-1·INT_MIN/MAX; 잘못된 색 enum; (0,0)의 잘못된 Cell enum | 모서리 Legal, 나머지 각각 OutOfBounds/InvalidColor/InvalidCell, 원본 보존. S |
| state_adapter | 모든 TSV 배치를 상태 이력으로 복원, 참가자 B를 흑에 배정, 후보는 TSV. 별도로 빈 상태 백 요청과 Draw 종료 후 흑 요청 | TSV 판정 동일·전체 상태 보존; UnexpectedColor/AlreadyFinished. 색 소유자와 돌 색 혼동 없음. S |
| move_limit (198) | `draw_pattern` 앞 198칸 복원, (14,14) 백 | 199번째 착수 Legal/None. G·J |
| move_limit (199) | 같은 패턴 앞 199칸 복원, (14,14) 백 | 200번째 착수 Draw/MoveLimit. 준비 배치도 이력에 포함. G·J |
| move_limit (200) | 같은 패턴 앞 200칸 복원, (14,14) 백 | Invalid/MoveLimitReached. 201번째 요청은 진행 불가. G·J |
| move_limit (승리) | 0~194번 칸을 패턴으로 채우고 (14,0~3)을 후보 색으로 배치해 199개 이력. (14,4) 흑/백 | Win, 200수 무승부보다 우선. 흑 사례는 교대 수량을 보장하지 않는 구조/정책 분리용 합성 상태. R9·J |
| move_limit (금수) | 0~193번 칸 패턴, (14,0~4) 흑으로 199개 이력. (14,5) 흑 | Forbidden, 장목이 200수 무승부보다 우선. 합성 상태의 정책 분리 검사. R9·J |
| full_board | 모든 (r,c)를 `(c/2+r)%2`가 0이면 흑, 아니면 백으로 채움. 한 칸씩 비워 원래 색을 후보로 분석, 225회 | Draw/BoardFull, 원본 보존. 연속 5·금수가 없는 교차 줄무늬 패턴. R10.1. 순수 보드 API이므로 200수 정책 없음 |

`full_board`는 백으로 채운 합성 보드의 마지막 빈칸 (7,7) 백, 마지막 행 (14,0~3)만 흑이고 (14,4)만 빈 합성 보드의 (14,4) 흑도 검사한다. 각각 백 5 이상·흑 정확한 5의 Win이며 만판 무승부보다 우선한다(R9~10). 기존 선의 과거 합법성을 검사하는 사례는 아니다.

수순 생성은 상태 저장 계약 검사용이며 실제 준비 오프닝을 선정·가져온 것은 아니다. 별도 심판 `play` 경계 비교는 198/199/200개의 준비 배치 길이를 넣고 다음 입력만 고정하여 실행한다. 199개 준비 배치 후 백의 200번째 승리와 200개 준비 배치 후 요청 0회를 확인한다. 정확한 입력·출력은 [비교 요약](verification/rules-comparison-summary.json) 및 [재현 방법](verification-rules.md)에 있다.

현재 이 명세의 미확정 기대값은 없다. 향후 심판 불일치 사례는 별도 미해결 표에 근거·영향을 기록하며 기대값을 맞추려고 테스트에서 숨기지 않는다. 최종 참가 심판 버전·실제 오프닝 출처는 미정이며 이 명세의 통과로 확정하지 않는다.
