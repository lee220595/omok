# 정책·가치 CNN + PUCT 참조 구현 (2026-10-28~29, 갱신2026-10-29)

기존 [SL 모델](supervised-learning.md)과 [무화면 환경](python-headless.md)을 재사용해 단일 프로세스·순차 PUCT를 구현했다. 시작 시 인계와 실제 e2b3c61 코드·체크포인트/DLL·테스트를 대조했고 제시된 구현 상태의 차이는 없었다. 현재 CNN은 쉬운 자체10판/95수의 기능 검증 모델이며 강한 AI가 아니다. 이번에는 가중치를 변경하거나 학습하지 않았다.

## 구현과 선택 규칙

- `python/omok_learning/mcts.py`: PyTorch에 의존하지 않는 PUCT/평가기 경계, 트리 통계, PUCTPlayer다. `ModelPlayer.evaluate()`는 기존 계약의 masked softmax 정책과 현재 착수자 가치 v를 제공한다. `prepare()`는 모델을 한 번 로드해 eval 상태로 두고 모든 forward는 inference_mode를 사용한다. 모델 단독 select도 같은 평가 경계를 사용한다.
- Edge의 P는 합법 수에 마스크를 적용한 정책 확률, N은 완료 방문 수, W는 **부모 node 착수자 관점의 가치 합**, Q=W/N이다. 미방문 Q=0이다. Node.N은 완료된 outgoing edge 방문 수의 합이다. `Q+c_puct*P*sqrt(N_parent)/(1+N_edge)`로 선택한다.
- 초기 N_parent=0에서는 모든 점수가0이므로 높은 P를 우선한다. 일반 점수 동률도 높은 P, 그다음 기본적으로 작은 행동 인덱스다. 평가의 최종 수는 **루트 N 최대 → 높은 P → 작은 행동 인덱스**다. 온도 샘플링/루트 노이즈는 없다.
- `tie_break='seeded'`를 명시하면 최종 동률 후보에서 지역 RNG로 선택한다. RNG는 매 search에 설정 seed로 초기화한다. 기본 index 모드에서는 seed가 결과에 영향을 주지 않는다. 전역 Python/torch RNG를 탐색 수 선택에 사용하지 않는다. 같은 고정 예산/상태/모델/소프트웨어·CPU 설정에서 수·방문/Q·평가 횟수의 재현을 확인했다. ID/실행 시간까지 같은 바이트를 보장하지 않는다.
- 루트 expansion의 신경망 평가1회는 simulation 예산과 별개다. **simulation S는 루트에서 한 번 이상 edge를 지나 leaf 평가 또는 실제 종료 결과를 역전파한 완료 순회 S회**다. 루트 N 합=S, `NN evaluations + terminal hits = S+1`을 검사한다. 일반 위치의 NN 호출은 보통 S+1이고 종료를 만나면 감소한다. 수 선택 전 루트 모델 가치도 반환하지만 final 수 선택은 방문 수로 한다.

논리 구조는 [AlphaZero 논문](https://arxiv.org/abs/1712.01815)의 정책/가치 기반 탐색이며, 여기의 고정 c_puct·초기 방문/동률·예산 의미는 위 구현 선택을 따른다. 전체 AlphaZero 학습 시스템을 구현했다는 뜻은 아니다. PyTorch의 [inference_mode 문서](https://docs.pytorch.org/docs/stable/generated/torch.autograd.grad_mode.inference_mode.html)에 따라 eval과 gradient 비활성화를 함께 적용한다.

## 관점·종료·복제 안전성

각 node에 실제 next_actor A/B를 저장한다. 비종료 leaf의 v는 그 leaf의 현재 actor 기준이다. 각 ancestor edge에 v를 기록할 때 그 node.actor와 leaf actor가 같으면 +v, 다르면 -v로 바꾼다. 이것은 일반 대국의 턴 교대마다 부호를 반전하는 것과 같으며 준비 돌 수의 홀짝으로 관점을 추측하지 않는다.

종료 leaf는 모델을 부르지 않는다. 실제 C++ winner/draw를 마지막 부모의 보존 actor와 비교해 +1/-1/0으로 만들고, ancestor마다 그 관점으로 변환한다. 종료 상태의 next_actor=None을 임의 흑/백으로 대체하지 않는다. aborted/blocked/결과 불명은 정상 terminal0으로 backup하지 않는다. 루트 종료는 요청 거부, 루트 합법 수0은 기존 runner의 blocked 경로, 가상 leaf의 blocked/실행 오류는 SearchError로 중단·기록한다. 금수는 기존 evaluate 마스크로 제외하며 탐색 중 마스크와 실제 적용이 충돌하면 오류로 처리한다. 정확한5목/금수/준비 포함200수의 기존 우선순위는 C++에 그대로 맡긴다.

라이브 환경은 읽기만 한다. 루트 평가용 base clone1개, simulation마다 base의 독립 clone1개를 만들고 선택 경로를 clone에서 실제 착수한다. C++ Controller 전체 값 복사를 재사용하고 Python에서 규칙을 구현하지 않았다. clone은 context manager로 정리한다. evaluator에는 clone만 전달하며 평가 전후 snapshot/Token 불변도 확인한다. 정상/예외 후 라이브 snapshot이 시작과 같은지 확인한다. 스레드 동시 쓰기는 기존 환경처럼 지원하지 않는다.

모든 합법 수를 edges에 둔다. 상위k개·근접 수·금수 판정 생략은 없다. P가 수치적으로0인 합법 수도 후보에는 남지만 유한 예산에서 방문을 보장하지는 않는다. 트리 재사용·전이표·undo/캐시·병렬·배치 추론은 추가하지 않았다. Clone/경로 재적용은 정확성을 우선한 기준 구현이며, 관측된 비용을 토대로 향후 최적화 여부를 결정한다.

## 설정·실행

추가 패키지 설치 없이 기존 선택 가상환경을 사용했다. Windows11 x64 10.0.26200, Python3.13.7 x64, torch2.14.1+cpu, Intel Core Ultra5 226V/논리CPU8이다. Torch CPU1스레드·deterministic algorithms, affinity 고정 없음. 기존 NumPy 미설치 경고는 남지만 numpy API를 쓰지 않으며 검증에 영향이 없었다. Linux/Colab/GPU/다른 버전·대회 시간/자원 조건은 미검증이다.

프로젝트 루트 PowerShell의 실제 실행 명령:

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

`evaluate games`는 기존 policy/ordered/random/self를 보존하고 `--player puct`, `--opponent policy`를 추가했다. --simulations/--c-puct/--search-seed/--tie-break/--compact/--source-id를 지정할 수 있다. search-seed 생략 시 대국 기본 seed를 사용한다. self는 같은 선택 방식끼리의 대국이며 가중치 평가기를 공유한다.

`SearchConfig` 기본값은 S=16, c_puct=1.5, seed23, index tie다. [JSON 예제](../configs/puct-cpu.json)는 Python에서 `SearchConfig(**json.load(...))`로 사용한다. CLI는 위 플래그로 지정한다. 검증용 초기값이며 최적값/대회 설정이 아니다. 시간 제한은 이번에 구현하지 않았다. 고정 simulation만 지원하며 한 수 시간 상한이나 동일 시간 비교를 충족했다고 표현하지 않는다.

comparison은 시작 전에 설정/모델·DLL·코드 해시/시작 조건을 manifest로 고정한다. 기본 시작은 빈 보드 DirectRenju와 자체 PreparedRenju h8i8g9이며 공식 오프닝이 아니다. 시작/예산마다 A=PUCT, B=policy로 2판에서 흑 소유자를 교대한다. game seed는23·24, PUCT seed는 두 판 모두23이다. 동일 checkpoint를 같은 평가기1개에서 호출한다. 모델 로드+warmup3회의 준비를 먼저 측정한 뒤 위치 측정·대국을 순차 실행한다. 각 trace를 독립 재생하고 루트 방문/예산을 검사한다.

동일 설정/소스/모델로 comparison을 다시 실행하면 완료 대국을 검증하고 건너뛴다. 미완료는 기존 runner의 attempt 보존·처음부터 재실행을 따른다. 설정/코드가 바뀌면 새 출력 폴더를 사용한다. progress.json에 완료 조합의 집계를 보관하며 기존 manifest를 다른 설정으로 조용히 덮지 않는다.

## 기록과 통계

기존 omok-trace-v1 필수 필드/보드 재생 의미는 유지했다. config.participants의 PUCT descriptor에 모델/evaluator 해시, SearchConfig, 선택 방식, noise/reuse/pruning=false를 기록한다. 각 action.inference에 다음을 추가한다.

- action, root_visits225, visit_distribution225(rootN/S), root_priors225, root_edge_q225, root_legal_count, root_q, 루트의 모델 value
- search_config, 실제 simulation 수, network_evaluations, evaluation_attempts, terminal_hits, expanded_nodes, transitions, clones, max_depth, network_ms
- milliseconds(로드를 제외한 search 본문), preparation_ms, 실제 이 select에서 발생한 model_load_ms, model_sha256

root_q는 선택 edge의 부모=루트 actor 관점이고 value는 루트 CNN 평가다. 둘을 같은 값으로 해석하지 않는다. network_ms는 텐서 생성/forward/마스크 정규화·검사 합이며 clone/규칙/트리 조작은 제외한다. runner의 mean_selection_ms는 외부 호출 시간이므로 준비도 포함할 수 있다. 비교표는 action의 milliseconds를 사용하고 준비를 분리했다.

SearchError에 부분 simulation/평가 시도/clone 등의 details를 붙여 기존 runner의 error 진단에 저장한다. 실패/취소를 draw/패배로 바꾸지 않는다. 루트 방문 분포는 미래 target용으로 보존하지만 RL 데이터 추출·경험 생성·업데이트는 이번에 구현하지 않았다. 재생은 모델을 호출하지 않고 실제 수순/결과를 검증하며, 탐색 메타데이터 자체의 재계산 검증과 구분한다. comparison은 별도로118번의 루트 방문/평가 수 정합성을 검사했다.

## 실제 측정

원래 모델 SHA256은 a7f9678942246028624ce694c35d16fc9d0e08b855f3fc385f4cac43b75be310이며 변경하지 않았다. 로드8.036ms, 로드+3회 warmup 등13.444ms이고 Python/torch 프로세스 import 시간은 별도로 제외한다. 위치마다3회 측정한 고정 위치 결과다.

| 선택 | 빈 보드 ms/수 | 준비3수 ms/수 | NN 평가/수 |
|---|---:|---:|---:|
| policy | 0.881 | 0.637 | 1 |
| PUCT S1 | 1.776 | 2.116 | 2 |
| PUCT S8 | 6.312 | 8.544 | 9 |
| PUCT S32 | 25.447 | 29.293 | 33 |

예산마다4판(2시작×흑백 교대)의 PUCT 관점 집계다.

| simulations/수 | 승/무/패 | PUCT 착수 수 | 평균 PUCT ms/수 | 평균 policy ms/수 | 실제 NN 평가/PUCT 수 |
|---|---|---:|---:|---:|---:|
| 1 | 2/0/2 | 42 | 1.582 | 0.516 | 1.952 |
| 8 | 3/0/1 | 39 | 6.519 | 0.738 | 8.308 |
| 32 | 3/0/1 | 37 | 36.732 | 0.702 | 30.243 |

12판234수·정상 완료12·오류0·완전 흑백 쌍6이며 모두 저장/독립 재생이 일치했다. 실제 대국의 NN 수가 S+1보다 작은 이유는 종료 leaf에서 모델을 부르지 않기 때문이다. 총 simulation1538, PUCT NN 호출1525, PUCT 착수118회다. 더 많은 simulation이 항상 더 좋은 결과나 일정한 지연을 보장한다는 주장이 아니다.

자세한 조건·시작별 결과/전이 수·해시는 [검증 JSON](verification/puct-search.json)에 있다. 고정 위치와 실제 대국은 보드 분포가 다르고, 3표본/예산별4판의 소표본이다. CPU/OS 시간 변동도 있다. 8/32예산의3승1패를 강함/실력 향상/Elo로 환산하지 않는다. 동일 시간의 공정 비교는 미실행이며 시간/NN 수는 후속 예산 설정의 기준 자료다.

## 정확성과 남은 범위

기존 GUI/C++/DLL 빌드·install 성공, CTest91개(기존90개+PUCT unittest8개를 묶은1개) 통과. 선택 학습 unittest6개도 통과했다. 통제 평가기로3단 부호·실제 턴 교대, 승리/패배/draw 값, 즉시 승리/필수 방어·200수 draw와 종료 NN 미호출, 전체 합법 후보·정규화/잘못된 평가 거부, 원본 불변, seed/방문 재현, simulation/NN/clone 정합, 오류 진단/trace 재생을 확인했다. 실제 모델에서도 한 번만 로드·eval/gradient 비활성·동일 방문/Q와 원본 불변을 확인했다. 코어 PUCT 테스트는 torch 없이 실행된다.

통제된 정책/가치와 유한 예산에서 해당 fixture를 찾았다는 사실이며, 유한 PUCT가 모든 전술을 반드시 발견한다는 증명이 아니다. 현재 모델/2시작/소표본·전체 재귀 최악 시간·엄격한 시간 예산·장기/공식 대회 적합성은 미검증이다. RL 업데이트, Gumbel, UCT 비교 엔진, 외부 기보, Rapfi, GUI, Gomocup 패키징, 다른 규칙은 이번 범위 밖이다. 후속 범위는 **같은 모델과 방문 분포를 사용하는 최소 자기 대국 강화학습 루프**이며 아직 시작하지 않았다. 최신 다음 제안/의존성은 아래 감시 설계와 개발 계획을 따른다.

## 감시 설계와 재사용 범위 (2026-10-29)

[두 감시자 계약](monitoring-contract.md)은 위 순차 PUCT/검사/통계를 재사용한다. root 방문·완료 simulation·NN/terminal/clone의 의미를 바꾸지 않으며 실패 중 부분 통계를 정상 성공 등식으로 검사하지 않는다. 통제 평가기 전술/부호 테스트는 알고리즘 회귀이고 실제 CNN의 검토된 필수 전술 세트·성능 감시 관문과 구분한다. [평가 계약](evaluation-contract.md)은 기보 재생/탐색 메타데이터 검사/탐색 재실행과 고정 후보/예산·중복을 구분한다.

기존 다음 작업 제안인 RL 루프는 계속 미구현이다. 이번 문서 작업 이후 가장 작은 다음 제안은 기존 검사 이벤트를 순차 감시 보고로 연결하는 범위이며 RL 업데이트를 자동 착수하지 않는다. 이후 RL은 [보상 계약](reward-contract.md)의 actor 관점·방문 분포 자격·SL 초기화/같은 단계 재개를 먼저 확인한다.
