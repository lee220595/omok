# 오목 프로젝트 개발 인계

갱신일 2026-10-23. 이번 시작 기준은 `c51a17bc787eb7c7fa2cd01fc95cef9c928f778c`이며 로컬 main·origin/main·origin/codex/state-model이 일치했다. 기존 상태 모델은 원격 푸시와 main 반영이 완료된 상태였고 작업 폴더는 깨끗했다. 이번에는 `codex/renju-rules`에서 작업했다. 최종 커밋·푸시 결과는 완료 보고 및 git log/원격으로 확인한다.

## 최신 승인과 우선 문서

사용자는 렌주 명세·판정 모듈·필요한 조사와 자동 테스트를 승인했다. 이 지시가 과거 인계의 규칙 구현 금지보다 우선한다. 실제 오프닝 진행·바둑판 GUI·AI 통신/프로세스·자체 AI는 이번 범위 밖이다. 승인된 기술 선택은 반복 승인받지 않는다.

- [AGENTS.md](../AGENTS.md): 확정 제품 정책·Git 운영.
- [상태 계약](state-model.md), [규칙 계약](rules-spec.md), [판정 사례](renju-cases.md): 다음 구현 전 읽을 API·경계.
- [규칙 검증](verification-rules.md), [심판 비교 요약](verification/rules-comparison-summary.json): 최신 실제 증거·미검증.
- [전체 계획](development-plan.md), [승인 기록](requirements-pending.md), [호환성 조사](compatibility-research.md), [매니저·엔진 후보](integration-candidates.md).
- [README](../README.md), [기술 선택](technology-decisions.md), 과거 [상태 검증](verification-state.md)·[기반 검증](verification-foundation.md).

지정 문서와 참조 문서는 모두 존재했다. 과거 기록의 “렌주 미구현”은 당시 범위이며 현재 규칙 소스와 최신 문서를 우선한다. 이번 조사에서 GomocupJudge check_forbid.py의 GPL-3.0-or-later 개별 헤더를 추가 확인했으나 저장소 전체의 라이선스 확정으로 확대하지 않았다.

## 현재 구현·미구현

| 부분 | 구현됨 | 미구현·제한 |
|---|---|---|
| app/gui | 기존 Win32 기본 창·한글 안내·메시지 루프·DPI 처리 | 바둑판·착수·시간·금수 표시·실제 대국. 이번 GUI 소스 무변경 |
| state | 독립 C++20 정적 라이브러리. 15×15 보드·좌표·색·A/B·색 배정·명시적 다음 색·결과 저장·225개 배치 이력·구조적 거부/초기화 | 규칙·입력 권한·직렬화·동기화. 이번 상태 소스/기존 테스트 무변경 |
| rules | 독립 C++20 정적 라이브러리. 구조 검사·국소 렌주·승리·장목·사사·재귀 삼삼 예외·만판·일반 대국 200수 정책 | 결과 적용·참가자 권한·과거 배치 합법성 검증·성능 보증 |
| opening/match/ai/records | CMake INTERFACE 경계 | 준비 배치 복원·Swap2 흐름·시계·실제 대국·통신/프로세스·기보/복기 전부 |
| tests | CTest 상태 13·규칙 39 = 52개. 고정 배치 35개·8대칭/회전·보존·수순·만판 | 실제 GUI 대국·오프닝·프로토콜·엔진 테스트 |
| 비교 도구 | 고정 GomocupJudge의 원본 함수·루프를 고정 입력으로 실행하는 Python 표준 라이브러리 스크립트 | 전체 매니저/서버·AI 프로세스·시간/자원 검사 |

규칙 헤더는 `src/rules/include/omok/rules/renju.hpp`, 구현은 `src/rules/renju.cpp`, 회귀는 `tests/rules_tests.cpp`·`tests/fixtures/renju.tsv`다. GUI는 아직 상태·규칙을 사용해 대국을 진행하지 않는다.

## 유지할 계약과 선택 이유

C++20·Win32 User32/GDI32·CMake 3.24+·MinGW-w64 x64/Ninja를 유지했다. 표준 C++ 상태·규칙은 GUI와 분리하고 Windows 배포용 기본 창과 기존 한글 경로 빌드 우회를 보존했다. 새 테스트 프레임워크나 GUI 의존성을 추가하지 않았다.

상태는 저장과 구조 검증만 맡는다. `commit_placement`는 입력 권한 검사가 아닌 저장 트랜잭션이며 다음 색을 명시한다. 참가자 A/B·돌 색·엔진 자신/상대와 실제 배치 참가자는 서로 다르다. `finish(Won{A/B})`는 색 미배정 상태에서도 외부 결과를 기록할 수 있다. 규칙 승리를 적용할 때 제어 계층이 색 소유자를 확인해야 한다. 이번 재검토와 기존 테스트에서 전체 상태 보존·종료 후 거부·225개 용량·미배정 결과 기록의 실제 결함을 발견하지 않아 구조 개선으로 확대하지 않았다.

`validate_board_candidate`는 구조 검사, `analyze_board`는 착수 전 보드를 사본으로 분석하는 순수 판정, `evaluate`는 정상 대국의 종료·다음 색·준비 배치 포함 총 200수 정책 어댑터다. 오프닝은 일반 대국 어댑터를 무조건 호출하지 않는다. 규칙은 후보를 지나는 선만 판단하며 기존 임의 보드의 과거 합법성을 보증하지 않는다.

정확한 흑 5는 동시 금수보다 우선, 백은 5 이상 승리다. 열린 사의 양끝을 중복 집계하지 않으며 삼의 확장 합법성은 깊이 제한으로 생략하지 않고 재귀 검사한다. Forbidden과 반칙패 결과 적용은 분리했다. 향후 match는 사본에 배치·차례·종료를 일관되게 적용한 뒤 공개하고, 심판 불일치는 기존 승인대로 중단·로그·승패 미확정 처리해야 한다.

공식 기준은 RIF 1996/1998 문서와 Gomocup 일반 렌주 수정 정책이다. 비교 심판은 기존 조사 커밋 `Gomocup/GomocupJudge@56cfd7dc1a0d1ec06d29f44a26931f375ad05991`을 고정했다. 비교 버전 선정은 최종 참가 연도·리그나 운영 심판 확정이 아니다. 기본 모드에 Swap2 후 렌주 조합을 추가하지 않았다.

## 실제 명령과 결과

GCC 13.3.0·MinGW-w64 11.0.1·CMake 3.29.3·Ninja 1.12.1, Python 3.13.7 사용. 프로젝트 루트에서 실행했다.

```powershell
$aliasParent = Join-Path $env:TEMP 'omok-rules-build'
New-Item -ItemType Directory -Force $aliasParent | Out-Null
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -AliasParent $aliasParent
ctest --test-dir .\build\native-release --output-on-failure --no-tests=error
python .\scripts\compare_rules.py --fetch --cache .\build\research\pinned-referee-verify --summary .\docs\verification\rules-comparison-summary.json
```

최종 빌드·링크·install 성공, 컴파일 경고 없음. CTest 52/52 통과. 실제 심판 함수의 2,264개 보드 및 원본 play 루프의 6개 수순 경계 대조에서 불일치 0. 보드 비교는 264개 명세 대칭·1,000개 임의 배치·1,000개 위협 밀집 배치다. 전부 합법 대국 수순이라는 의미는 아니다.

원본 check_forbid와 원본 ai_match의 __init__/make_move/play를 선택 실행했고 다음 입력만 고정했다. 준비 배치 길이 198/199/200을 주어 다음 요청 수 2/1/0, 200번째 백 승리 우선도 확인했다. 실제 AI 프로세스는 실행하지 않았다. 원본 소스는 build 캐시에만 있으며 제품에 복사·번역·배포하지 않는다. Python은 선택적 비교에만 필요하고 기본 빌드/CTest에는 필요 없다. 상세 해시·입출력·재현 범위는 검증 기록과 JSON을 따른다.

최초 기본 CTest는 과거 TEMP 별칭의 exe를 못 찾아 13개 Not Run, 기본 빌드도 기존 연결의 대상 불일치로 중단했다. 기존 연결을 보존하고 위의 별도 AliasParent로 해결했으며 구현 전 상태 13개를 먼저 통과했다. 중간 회귀에서는 두 fixture의 잘못 입력한 좌표와 이를 포함한 어댑터 검사 3개가 실패했고, 의도한 배열을 바로잡아 공식 근거·심판과 재검증했다. 실패를 성공으로 보고하지 않는다.

## 남은 쟁점·미검증과 이유

- 확인한 사례에서 구현을 막는 규칙 충돌은 없다. 모든 배치·모든 재귀 깊이의 완전 일치와 최악 성능·225점 금수 힌트 반응성은 미검증이다. 새 불일치는 독립 사례로 보존하고 근거 없이 기대값을 정하지 않는다.
- 최종 참가 연도·리그·운영 심판 버전·준비 오프닝 데이터 출처/사용 조건은 미정이다. 200수 계산과 우선순위는 선정한 고정 심판에서 확인했으나 미래 운영 버전 검증을 대체하지 않는다.
- 전체 매니저 client/server·실제 엔진·통신·시간/자원·오류 처리는 미구현 또는 실행 환경 미구성으로 검증하지 않았다. Gomocup 통신 호환·대회 규칙 전체 일치·매니저 채택 완료로 표시하지 않는다.
- MSVC·비Windows·개발 도구 없는 별도 Windows 10/11 PC·메모리 sanitizer 환경이 없어 검증하지 않았다.
- 기본 GUI exe는 함께 빌드했지만 이번에는 창 표시/닫기·DPI·장시간 자원을 재검증하지 않았다. 과거 GUI 실행 결과와 구분한다. ZIP도 재생성하지 않았다.
- 빌드의 기존 ASCII junction·상대 리소스 컴파일·Ninja 메타데이터 처리를 유지한다. 실행 중 GUI의 파일 잠금은 닫은 뒤 빌드한다. 기본 TEMP 별칭이 충돌하면 임의 삭제하지 말고 다른 AliasParent를 사용한다.

## 다음 단계

1. 현재 상태·규칙 모듈과 52개 CTest를 기반으로 **준비 오프닝 복원·본게임 전환** 단계에 진입할 수 있다. 근거는 명세 구현·원본 상태 보존·수순 정책·고정 심판 대조 통과다. 전체 게임/대회 호환성이 완료됐다는 뜻은 아니다.
2. 준비 배치 데이터의 출처·사용 조건을 확인하고 수순 순서대로 저장·분석·다음 색 설정을 구현한다. 원본 데이터의 규칙과 최종 목표 대회를 혼동하지 않는다.
3. 별도 SWAP2BOARD 요구를 위한 오프닝 상태 전환과 참가자/색 경계를 명세하되 대회 기본 오프닝과 혼합하지 않는다. 미정 GUI 입력 방식은 실제 해당 화면 설계가 필요할 때 확인한다.
4. 이후 match가 참가자 권한·판정 결과의 원자적 적용·오류/중단 정책을 맡고 GUI·외부 AI·기보를 순서대로 연결한다. 사용자 최신 지시 범위에 맞춰 진행한다.
5. 실제 엔진/매니저 통합 단계에서 버전·설정·통신 로그·결과를 추가 검증한다. 통상 기술 선택은 자율 진행하고 필수 규칙/제품 범위 변경만 사용자에게 확인한다.
