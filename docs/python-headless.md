# Python 무화면 자동 대국 (2026-10-28)

기존 C++ state/rules/opening/match를 GUI·Rapfi 없이 같은 Python 프로세스에서 호출한다. 모델·MCTS·학습·보상 업데이트·제출 엔진 프로토콜은 포함하지 않는다. ordered/random은 검증용 기준 플레이어다.

## 설치와 실행

검증 환경: Windows 11 x64 10.0.26200, Python 3.13.7 x64, GNU g++ 13.3.0 MinGW-w64 ms vcrt POSIX SEH, CMake 3.29.3, Ninja 1.12.1. 새 pip 패키지나 학습 프레임워크 설치는 없다. 기존 GCC/MinGW 런타임 고지는 [배포 고지](../THIRD_PARTY_NOTICES.md)에 따른다. Python은 사용자 설치이며 동봉하지 않는다.

프로젝트 루트 PowerShell에서 실제 실행한 명령:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -AliasParent C:/Users/leesn/AppData/Local/Temp/omok-headless-build -Headless
$env:PYTHONPATH='python'
python -m unittest discover -s python -p test_headless.py -v
ctest --test-dir build/native-release --output-on-failure --no-tests=error
python -m omok_env.runner runs/smoke --games 2 --seed 17
python -m omok_env.benchmark runs/benchmark-final --samples 30 --games 4
```

AliasParent는 존재하는 ASCII 경로를 사용한다. 이번 임시 부모 폴더는 New-Item으로 먼저 생성했다. 기존 alias가 다른 대상이면 보존하고 새 부모 폴더를 사용한다. 기본 build.ps1은 DLL/Python 없이 기존 빌드·GUI 설치를 유지한다. -Headless는 CMake OMOK_BUILD_HEADLESS=ON을 설정한다. Python 인터프리터를 찾으면 headless.python CTest를 추가한다. DLL은 build/native-release/bin/libomok_headless.dll이며 dist GUI ZIP에는 넣지 않는다. 다른 DLL은 OMOK_HEADLESS_LIBRARY 환경 변수 또는 Environment(library=절대경로)로 지정한다.

## 연결 선택과 책임

선택적 C ABI DLL + ctypes다. CPython 확장 ABI/pybind11 설치 없이 동일 프로세스에서 핸들을 재사용하고 정확한 Controller 값 복제로 탐색용 clone을 제공한다. bridge.cpp는 C++ 판정/수명/고정 버퍼 경계, omok_env는 Python 타입/식별자/관측, records는 저장/검증, runner는 플레이어/반복/재개를 맡는다. Python은 렌주를 판정하지 않는다. C ABI 버전은 omok_abi()=1이며 버전 불일치는 로드 거부한다. raw C ABI는 내부 연결용이고 공개 사용자 API는 아래 Python API다. 잘못된 raw 포인터는 보호하지 않는다.

환경은 단일 작성자이며 동시 호출을 지원하지 않는다. C++ 예외는 ABI에서 오류로 바꾸고 메모리 해제는 DLL 내부에서 수행한다. 코어의 Windows 의존성은 추가하지 않았고 DLL export/기존 루트 빌드만 Windows 경계다. Linux·Colab·MSVC·다른 Python 버전·클린 Windows는 미검증이다.

## API와 좌표·관측

```python
from omok_env import Environment
with Environment(black_owner=0) as env:
    state = env.snapshot()
    observation = env.observation()
    legal = env.mask()
    action = next(i for i, ok in enumerate(legal) if ok)
    decision = env.step(action, actor=state['next_actor'], expected=state['token'])
    with env.clone() as branch:
        branch.reset(compact='h8i8g9', source_id='self-authored-v1', black_owner=1)
```

- Environment/reset: 빈 compact는 DirectRenju, 비어 있지 않으면 PreparedRenju 복원. 준비는 기존 compact 로더의 순서·색·합법성 검사를 통과해야 한다. source_id는 출처 식별자이며 공식/권리 확인 증명이 아니다. 실패한 reset은 기존 환경을 보존한다.
- action_of(row,column), coordinate_of(action): 0..224 ↔ 0..14 행/열. 원점 왼쪽 위, 행 아래/열 오른쪽. action=row*15+column. A1=0, O15=224. compact의 h8은 row7,column7이며 protocol x=column,y=row와 구분한다. bool/실수/범위 밖 정수는 거부한다.
- snapshot(): 절대색 board(빈0/흑1/백2), next_color(흑0/백1 또는 None), next_actor(A0/B1 또는 None), black/white_owner, history, 준비 수, source_id/start_mode, profile/version, phase, result/winner/end_reason/rule_reason, event_count, token을 반환한다. 종료 후 다음 착수자는 None이다. 준비 history.actor는 None, 일반 수는 실제 참가자다. 시도 금수 돌은 성공 history/보드에 없다.
- observation(): 독립 소유 array('f') 900개, float32 C순서 [1,4,15,15]로 reshape할 수 있다. 내 돌·상대 돌·현재 흑 여부·남은 수 비율 순서다. 종료/닫힘이면 None. 내 돌은 A가 아니라 현재 착수 색 기준이다. 증강/정책·가치 모델은 아직 없다.
- mask(): 독립 tuple bool225. rules::evaluate의 Legal/Win/Draw만 참. 금수·점유·종료는 false. 정상 5목 우선순위 그대로다. 상태를 바꾸지 않는다. 합법 수0인 진행 상태는 호출자가 blocked로 격리하며 runner는 aborted/blocked 기록을 남긴다. 패스/임의 draw는 없다.
- step(action, actor=None, expected=None): 기본 현재 actor/Token을 동기적으로 사용한다. 비동기 사용자는 snapshot token을 명시한다. token=[환경 고유 game_id, C++ session, revision]. reset/clone의 고유 ID로 다른 환경/옛 대국 입력을 거부한다. 판정 verdict/reason을 반환한다. 구조/범위/권한/오래된 요청은 ValueError로 거부하며 상태/이벤트/Token 보존. 네이티브 실행 예외는 RuntimeError다. 금수는 예외가 아니라 forbidden 판정·백 승리/종료/버전 갱신이다.
- clone(): Controller의 보드·오프닝·이력·결과·이벤트까지 정확한 값 복사. 독립 ID를 부여한다. branch 착수/reset/dispose는 원본에 영향이 없다. undo/캐시 없음.
- abort(): 진행 상태를 기존 close 정책으로 aborted/closed 처리. 정상 승무패를 만들지 않으며 이미 종료된 결과는 보존한다. dispose()/with는 네이티브 메모리를 정리한다. 읽기 반환값 변경은 내부에 영향이 없다.

지원 프로필은 renju15-fixed200-v1 하나다. 기존 evaluate의 고정 200수(준비 포함)를 사용하며 승리/금수 우선 후 정상 200번째 수는 draw다. remaining_moves=200-placements, 관측 채널3=remaining_moves/200. GUI/외부 엔진의 기존 시계/자원은 바꾸지 않았으며 이 runner에는 대회 시계/자원 제한이 없다. --max-actions는 별도 실행 중단이며 200수 규칙 무승부와 다르다. 다른 프로필 요청은 거부한다.

## 실행기·안전 저장·재개

runner의 --games, --seed, --players ordered random, --compact, --source-id, --black-owner, --max-actions를 지원한다. 참가자 A/B의 플레이어 종류는 고정하고 index마다 흑 소유자를 교대한다. 같은 compact를 두 판씩 공유하며 게임 seed=기준 seed+index다. random.Random으로 오름차순 합법 목록에서 선택하므로 같은 코드/DLL/Python/설정/시드에서 수순이 재현된다. Python RNG 버전·컴파일러/플랫폼 변경까지 같은 수순을 보증하지 않는다. 실제 AI 강함 평가가 아니다.

manifest.json은 설정·Git commit/dirty·소스 SHA256·DLL SHA256을 고정한다. 동일 폴더의 설정/코드/바이너리 변경은 재개 거부한다. 완료 파일은 독립 재생 검증 후 한 번만 집계하며 재대국하지 않는다. 중단/오류 파일은 attempt-N으로 보존한 뒤 같은 seed의 대국을 처음부터 재실행한다. 중간 RNG/환경 위치에서 이어두기는 지원하지 않는다. 동시 실행은 서로 다른 output 폴더를 사용한다.

대국 파일은 같은 디렉터리 임시 파일→flush/fsync→os.replace로 완결 단위 저장한다. Ctrl+C/명시 cap은 interrupted, 실행 예외는 error, 합법 수0은 blocked이며 diagnostic에 입력/원인을 남긴다. 시작 실패는 별도 omok-run-failure-v1 파일로 남기고 게임 결과를 만들지 않는다. 완료된 파일은 이후 중단에도 남는다. 강제 프로세스 종료/전원 손실 시 현재 진행 대국은 유실될 수 있으며 다음 실행은 저장 완료 파일만 건너뛴다. 저장장치 전원 장애의 완전한 디렉터리 내구성까지 보증하지 않는다.

## 기록 형식과 재생

JSON UTF-8 omok-trace-v1이다. 추가 JSON 라이브러리가 없다. 형식/프로필 버전, game_id, 실행 config/seed/code_version, 원본 start, initial, actions, status/diagnostic, final을 담는다. 각 action은 action/row/column/color/actor/decision 및 after 스냅샷을 저장한다. 이는 크기를 줄인 학습 샘플이 아니라 판정 검증용 원본 trace다. 준비 돌은 initial/start에, 일반 착수와 금수 시도는 actions에 구분한다. 오류 입력은 diagnostic.pending에 보존한다. 정책/가치 자격과 보상은 생성하지 않는다.

일부 필드 예시(실제 smoke 기록은 runs/smoke/game-000000.json):

```json
{"schema":"omok-trace-v1","profile":"renju15-fixed200-v1","profile_version":1,
 "game_id":"game-000000","seed":17,"status":"completed","diagnostic":null}
```

이 발췌만으로 재생할 수는 없다. 실제 파일에는 위 전체 필드가 포함된다. 완전한 소형 예제 [headless-forbidden-v1.json](../tests/fixtures/headless-forbidden-v1.json)은 자체 작성 준비8수 뒤 금수 시도와 반칙패를 저장하며 테스트에서 재생한다.

```powershell
$env:PYTHONPATH='python'
python -c "from omok_env.records import replay; print(replay('runs/smoke/game-000000.json')['result'])"
```

replay는 별도 Environment를 생성하여 시작 복원→모든 action 판정/색/actor/좌표/중간 스냅샷 비교→필요 시 abort→최종 보드·차례·이력·결과/사유 비교를 수행한다. game token은 재생에서 새로 발급해 논리 비교에서 제외한다. 원본 환경 인자를 받지 않으므로 손상 파일이 기존 환경을 바꿀 수 없다. 미지원 형식/프로필·손상·불일치는 예외로 거부하며 조용히 수정하지 않는다.

## 실제 검증·성능

기본 GUI/C++ 빌드·install 성공, 기존 CTest89개 통과. 새 Python 테스트는 좌표225왕복·관점/준비·마스크와 복제 실제 판정900개 대조·정확한5목/금수/200수·종료 후 거부·잘못된 입력/Token·독립 복제·100회 수명·동일 시드/재개·완료/중단/오류·손상/재생을 검증한다. 착수 커밋 직후 취소도 별도 주입한다. 기존 rules 회귀의 재귀 예외/대칭/200번째 승리·금수 우선 검증도 통과했다. 전체 결과는 [인계](handoff.md)에 기록한다.

최종 코드의 소표본 측정: Intel Core Ultra 5 226V, 논리CPU8, Windows11, Release, 단일 프로세스 순차, affinity 고정 없음. 30회 평균 ms: 준비3수 reset 0.00848, snapshot 0.01421, 관측0.05751, 준비3수 mask0.06353, 위협8수 mask0.16688, step0.02711, clone+dispose0.00556. step은 미리 생성한 독립 clone에 한 번씩 적용하며 Python Token/상태 조회 비용을 포함한다. 관측은 Python float32 배열 생성까지 포함한다.

무작위/무작위 4판(seed20261028), 531수/4승리, 저장·재생·manifest/요약 포함 총1.01438초, 523.47수/초·3.943판/초. 내부 대국 루프0.78005초와 전체 wall을 구분했다. 소수 표본 진단이며 장시간/밀집 최악 재귀/대회 실력 보증이 아니다. 원본 측정 출력은 Git 제외 runs/benchmark-final/benchmark.json에 있다. 마스크는 다른 단일 호출보다 크지만 이번 전체 경로에서 정확성 유지가 우선이며, 매 수 한 번 요청하므로 캐시를 추가하지 않았다. 밀집/반복 조회 병목이 확인되면 Token 기준 캐시를 검토한다. 대량 병렬화/GPU/벡터화는 후속 범위다.

다음 작업 하나는 이 환경 위에 작은 정책·가치 모델의 지도학습과 추론을 연결하는 것이다. 데이터 권리/라벨·분할과 모델 계약을 먼저 확인하고 별도 승인 범위로 진행한다. 이번에는 시작하지 않았다.
