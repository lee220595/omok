# 오목 프로젝트 개발 인계

갱신2026-10-29, 작업2026-10-28~29. 이번 승인 범위는 **기존 CNN의 P/v를 쓰는 순차 PUCT·실행기/기록 연결·정확성/고정 예산 비교·문서 갱신**이다. 모델 가중치/SL 데이터는 변경하지 않았고 RL 업데이트·Gumbel/UCT 비교 엔진·외부 기보·Rapfi·GUI·Gomocup 패키징·다른 규칙은 구현/실행하지 않았다. 다음 작업도 자동 시작하지 않는다. 이전 [SL 인계](handoff-before-puct-2026-10-29.md)를 보존한다.

## 시작 상태와 현재 코드

시작e2b3c61/codex/supervised-policy-value, 추적/미추적 변경 없음. AGENTS/README/인계·개발 계획·모델/환경/평가 계약과 실제 구현/테스트를 대조했다. 제시된 상태(기존 무화면/clone·SL/체크포인트·모델 대국/trace·쉬운 자체10판95수 모델·MCTS/RL 미구현)와 차이는 없었으며 checkpoint/DLL도 존재했다. fetch 후 main과 기존 의존 이력12커밋앞/뒤0을 확인하고 이 기반에서 codex/puct-search를 만들었다. main 통합/기존 브랜치 삭제/강제 푸시는 하지 않는다.

- python/omok_learning/mcts.py: stdlib 기반 PUCT 코어/평가기 경계, Node/Edge의P/N/W/Q, 실제 actor 관점 backup/실제 종료값, 전체 합법 후보, root 방문/분포·예산/NN/전이/clone 통계와 PUCTPlayer. 매 simulation C++ clone/경로 착수로 실제 환경을 바꾸지 않는다.
- player.py: prepare로 모델1회 로드, evaluate로 기존 checked masked softmax/current-actor v를 정책 단독과 PUCT가 공유한다. eval/inference_mode 유지, 로드 시간을 선택 시간과 구분한다.
- evaluate.py: 기존 policy/ordered/random/self 보존, --player puct/--opponent policy와 simulation/c_puct/search-seed/tie/시작 플래그 추가. compare_search.py: 시작 전 manifest 고정→같은 checkpoint 평가기 공유→고정 위치 측정→2시작·흑백 교대 비교→trace 독립 재생/통계 검사·진행/최종 보고 저장.
- omok_env/runner.py: SearchError.details를 기존 오류 진단에 보존한다. 기존 trace v1의 실제 수순/종료/재개 의미는 유지하고 action.inference에 탐색 메타데이터를 추가했다. C++/GUI/규칙 소스는 변경하지 않았다.
- python/test_mcts.py의 통제 평가기 검사8개, test_learning.py의 실제 모델 PUCT/로드1회/eval 검사1개 추가(선택 학습 총6개), headless CMake에 torch 없는 PUCT CTest 등록, configs/puct-cpu.json과 [실행 안내](puct-search.md)·[검증 JSON](verification/puct-search.json)·README/계약/계획을 갱신했다.

## 주요 설계·안전성

선택은 Q+c_puct*P*sqrt(N_parent)/(1+N_edge), 미방문Q0, N_parent=완료 outgoing edge 방문 합이다. N_parent0일 때 모든 점수0이므로 높은P로 초기 선택한다. 점수/최종 방문 동률은 높은P 후 기본 작은action, 선택적으로 seeded tie다. 기본 seed는 기록되지만 index 모드의 수 선택에 영향을 주지 않는다. seed 모드는 search마다 지역 RNG를 초기화하며 전역 RNG/루트 노이즈를 사용하지 않는다.

Edge Q/W는 부모 actor 관점, 비종료 leaf v는 leaf actor 관점이다. 각 ancestor actor와 비교해 부호를 바꾸며 준비 돌 수의 홀짝으로 추정하지 않는다. 종료 leaf는 NN 대신 C++ winner/draw와 보존된 actor로±1/0을 계산한다. aborted/blocked/실행 실패를0으로 backup하지 않는다.

루트 expansion1회는 simulation에 포함하지 않는다. S simulation은 루트를 출발해 leaf를 평가/역전파한 완료 순회S회다. root N합=S, NN evaluations+terminal hits=S+1, clone=S+1이다. 라이브 snapshot은 읽기만 하고 base clone+simulation clone만 변경한다. evaluator가 clone을 바꾸거나 stale Token/불법 확률/NaN을 반환하면 오류다. 원본 환경 불변을 정상/예외 후 확인한다. 상위k/근접 후보·금수 판정 생략·트리 재사용·배치/병렬/전이표/undo 캐시는 넣지 않았다. 시간 제한은 이번 미구현이며 고정 simulation만 지원한다.

## 실제 검증과 비교

Windows11 x64 10.0.26200·Python3.13.7 x64·torch2.14.1+cpu, Intel Core Ultra5 226V/논리CPU8. Torch CPU1스레드·deterministic algorithms, affinity고정 없음. 기존 GCC13.3.0/CMake3.29.3/Ninja1.12.1 빌드/install 성공. 새 의존성 설치 없음. NumPy 미설치 import 경고는 기존대로 남지만 numpy API를 쓰지 않으며 모든 실행이 성공했다.

- 최종 CTest91개 통과8.90초: 기존90개+torch 없는 PUCT unittest8개를 묶은1개. 선택 학습 unittest6개 통과6.108초. Python compileall도 통과했다.
- 통제 평가기로3단 backup·실제3턴 leaf 부호, winner 양쪽/패배/draw·즉시 승리/필수 방어·200수 종료에서 NN미호출, 전체 합법 후보/정규화·불법 평가/상태 변경 거부·원본 불변·같은seed/root방문·S/NN/clone/전이 정합·실패 진단/독립 재생을 확인했다. 실제 CNN에서 모델로드1회·eval/gradient비활성·반복 방문/Q/원본 보존도 확인했다. 유한 예산에서 모든 전술 발견을 보증하는 테스트는 아니다.
- 1simulation smoke2판 정상 종료·저장/재생 후, S1/8/32를 각4판(빈 보드와 자체 h8i8g9 각각 흑백 교대2판) 순차 비교했다. A=PUCT/B=같은 모델policy, game seed23/24, search seed23, c_puct1.5/index tie/no noise. 총12판234수·정상완료12·오류0·완전쌍6, 모두 독립 재생 일치. 작은 두 시작의 개발 평가이며 공식 오프닝/최종 실력 평가가 아니다.

| S | PUCT 승/무/패 | PUCT 착수 | 평균PUCT ms | 평균policy ms | NN평가/PUCT 수 |
|---|---|---:|---:|---:|---:|
| 1 | 2/0/2 | 42 | 1.582 | 0.516 | 1.952 |
| 8 | 3/0/1 | 39 | 6.519 | 0.738 | 8.308 |
| 32 | 3/0/1 | 37 | 36.732 | 0.702 | 30.243 |

실제 simulation1538/PUCT NN호출1525/PUCT착수118, terminal leaf의 NN생략으로 S+1보다 적은 평균이다.118개 저장 분포의 rootN/NN 정합도 별도 확인했다. 고정 위치3표본 평균(빈보드/준비3수): policy0.881/0.637ms, PUCT S1 1.776/2.116ms·NN2회, S8 6.312/8.544ms·NN9회, S32 25.447/29.293ms·NN33회. 준비는 모델로드8.036ms·로드+3회warmup 등13.444ms이며 프로세스 import 시간은 제외했다. 실제 대국과 고정 위치의 보드 분포/OS 변동이 다르므로 시간을 섞어 동일 시간 우위로 주장하지 않는다.

모델은 기존 자체10판95수로 기능만 검증한 CNN1,050파라미터다. SHA256 a7f9678942246028624ce694c35d16fc9d0e08b855f3fc385f4cac43b75be310 유지. 원본 SL 모델/가중치·데이터는 변경하지 않았다. 8/32의3승1패도 실력 향상/Elo 증거가 아니다. 전체 조건·시작별 결과/소스/DLL 해시와 준비/통계는 검증 JSON을 따른다.

## 실행 명령·저장·재개

```powershell
$env:PYTHONPATH='python'
$env:TEMP=(Resolve-Path build/pip-temp).Path
$env:TMP=$env:TEMP
build/learning-venv/Scripts/python.exe -m omok_learning.evaluate games runs/sl-release-trained/best.pt runs/puct-smoke --player puct --opponent policy --simulations 1 --games 2
build/learning-venv/Scripts/python.exe -m omok_learning.compare_search runs/sl-release-trained/best.pt runs/puct-comparison-final --simulations 1 8 32 --games 2 --samples 3 --seed 23 --c-puct 1.5
python -m unittest discover -s python -p test_mcts.py -v
build/learning-venv/Scripts/python.exe -m unittest discover -s python -p test_learning.py -v
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -AliasParent C:/Users/leesn/AppData/Local/Temp/omok-headless-build -Headless
ctest --test-dir build/native-release --output-on-failure --no-tests=error
```

기본값/동률·record 필드·예산 단위는 실행 안내에 있다. trace는 action.inference의root_visits/visit_distribution/P/Q·선택수·실제simulation/NN/시간·모델해시를 보존한다. 재생은 실제 수순/보드/결과를 확인하며 모델/탐색을 다시 실행해 메타데이터까지 증명하는 것은 아니다. 방문 분포는 미래 target을 위해 저장하지만 이번에RL경험/업데이트로 전환하지 않았다.

comparison manifest에 설정·시작·코드/모델/DLL을 먼저 고정한다. 같은 설정의 완료 기록은 독립 검증 후 건너뛰고 미완료는attempt 보존 후 처음부터 실행한다. 코드/설정/모델 변경은 새 출력 폴더를 사용한다. 체크포인트·raw trace·대량 로그·빌드/가상환경은 Git 제외, 작은 설정/테스트/요약 검증만 추적한다.

## 미검증·다음 작업·Git 완료 지점

실제 고수 기보/학습 향상·광범위 전술/재귀 최악·동일 시간 예산/하드deadline·장시간/다른OS/Python/GPU·Gomocup수신/대회 적합성은 미검증이다. 최적화는 현재 정확한 순차 기준과 측정을 확보한 뒤 필요할 때 별도 검토한다. Rapfi/GUI 수동 실행은 하지 않았다. 새로운 프로필/규칙은 없다.

다음 작업 하나는 **같은 모델과 방문 분포를 사용하는 최소 자기 대국 강화학습 루프**다. 보상/오류자격·SL초기화/같은단계재개·가치관점 검증을 별도 범위로 진행하며 이번에 자동 시작하지 않는다.

codex/puct-search에서 관련 코드/작은 테스트·설정·문서·검증 요약만 커밋·일반 푸시한다. 기존 변경/브랜치/이력은 보존하고 main통합/강제푸시/삭제하지 않는다. 최종 커밋ID/실제 푸시는 완료 보고에 남기며 문서만 반복 커밋하지 않는다. 문서 상대 링크346개 누락0·한자/가나 오타 검색·git diff --check를 확인했고 검증 보고서의 모델/DLL 해시·전체 예산 합계도 대조했다. 현재 승인 범위의 구현/검증/저장 후 중단한다.
