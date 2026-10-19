# 상태 모델 검증 기록

검증일: 2026-10-19. 시작 기준은 `main`·`origin/main`의 `3aba5a69403ea29f6515b2fc155ddcf183ab83e1`. 기존 인계의 기준 `c2a7dd9` 이후에는 인계 문서·README 링크 추가만 있었다. 미커밋 변경 없이 `codex/state-model`에서 작업을 시작했다. 사용자가 상태 모델과 관련 자동 테스트 구현을 명시 승인했다.

## 실제 수행

기존과 같은 Windows 호스트에서 GCC 13.3.0·MinGW-w64 11.0.1·CMake 3.29.3·Ninja 1.12.1을 사용했다. 프로젝트 루트에서 다음을 실행했다.

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1
ctest --test-dir .\build\native-release --output-on-failure --no-tests=error
```

- **빌드·링크·install 성공, 종료 코드 0.** 상태 라이브러리·상태 테스트·기본 GUI가 모두 빌드됐다. 컴파일 출력에 경고가 없었다. `scripts/build.ps1`은 변경하지 않았다.
- **CTest 13개 중 13개 통과, 실패 0.** 실행 로그의 전체 실시간은 0.60초였다. 테스트 없는 구성을 성공으로 처리하지 않도록 `--no-tests=error`를 사용했다.
- Release 빌드에서도 CHECK가 실행되며 `assert`/NDEBUG에 의존하지 않는다. 외부 테스트 패키지를 받지 않았다.

| CTest 이름 | 실제 검증 내용 |
|---|---|
| state.initial_state | 225칸 빈 보드, 빈 이력, 색 미배정, 다음 흑, 진행 중. 읽기 전용 접근·타입 분리 정적 검사 |
| state.coordinates | 유효한 225개 좌표의 인덱스 왕복·행열 의미, 음수·15·INT_MIN/MAX·초과 인덱스 거부 |
| state.boundaries | 네 모서리 허용, 서로 뒤집힌 `(2,11)`·`(11,2)` 독립 보존 |
| state.rejected_requests | 판 밖·중복·잘못된 색/참가자·다음 색·예정 색 불일치 거부, 매번 전체 GameState 보존 |
| state.color_assignment | 미배정과 두 배정 방향의 A/B↔흑/백·다음 참가자·잘못된 enum 조회 |
| state.relative_stones | 두 참가자 관점의 자신/상대, 미배정·잘못된 관점/색 거부 |
| state.recoloring | 기존 배치 후 색 소유 변경 시 돌·이력의 실제 참가자·다음 색 유지, 다음 참가자만 새 배정으로 계산 |
| state.history | 위치·색·실제 참가자의 입력 순서, 거부 요청 제외, 복사본 저장소 독립 |
| state.explicit_turn | 흰색 초기 차례, 명시한 다음 색 유지. 입력 권한·자동 교대가 상태의 책임이 아님 |
| state.terminal_results | 승자·무승부·중단 결과 기록, 종료 후 배치·배정·중복 종료 거부 및 상태 보존 |
| state.rejected_results | 진행 중 결과·잘못된 승자 거부 및 보존, 외부 결과 기록이 보드 승리 감지가 아님 |
| state.reset | 진행·종료 상태의 전체 초기화, 잘못된 초기 설정 거부 시 보존, 배정·흰색 시작 재설정 |
| state.no_adjudication | 225개 배치를 저장하고 5목/장목·200수·만판을 자동 판정하지 않음, 만판 후 중복 거부·초기화 |

## 기본 GUI 회귀 확인

- `git diff -- scripts/build.ps1 src/gui src/app` 출력이 없어 기존 빌드 스크립트·창 구현의 무변경을 확인했다. 루트 CMake는 상태와 테스트를 추가하고 공통 컴파일 옵션만 함수로 정리했다.
- 새로 빌드·설치한 `dist/omok/omok.exe`를 computer-use의 `sky.launch_app`으로 실행했다. 대상 창 제목과 한글 기본 안내를 확인했다. 화면 일부가 다른 앱에 가려져 있어 가려진 부분의 표시를 검증했다고 주장하지 않는다.
- 대상 창 접근성 트리의 닫기 단추로 종료하고 해당 제목 창이 목록에서 사라짐을 확인했다. GUI 프로세스 종료 코드 자체는 수집하지 않았다. 바둑판 GUI는 추가하지 않았다.

## 독립성과 배포 경계

```powershell
& 'C:\Program Files\mingw64\bin\objdump.exe' -p build\native-release\bin\omok_state_tests.exe |
    Select-String 'DLL Name|Magic|Subsystem'
```

테스트 실행 파일은 PE32+ x64 콘솔 형식이며 import는 KERNEL32.dll·msvcrt.dll만 확인했다. User32/GDI32 또는 별도 GCC 런타임 DLL에 의존하지 않았다. 이는 코드·CMake의 GUI 독립성 확인을 보완하며 비Windows 실행 검증을 뜻하지 않는다. 테스트 실행 파일·정적 라이브러리는 일반 GUI install 패키지에 동봉하지 않는다.

기존 install 동작은 재실행했으나 이번에는 ZIP을 새로 만들지 않았다. 남아 있는 `omok-0.1.0-windows-x64.zip`을 이번 상태 구현의 새 산출물로 표시하지 않는다. 과거 [기반 검증 기록](verification-foundation.md)의 바이너리 해시도 재빌드 이후 현재 파일을 식별하는 값으로 사용하지 않는다.

## 미검증·남은 제한

- 실제 일반 대국의 착수 권한, 오프닝의 여러 돌 배치·색 선택, 렌주 금수·승리·200수·만판 무승부 판정은 구현·테스트하지 않았다. `commit_placement`의 구조적 허용을 게임 규칙으로 해석하지 않는다.
- 화면 픽셀/표시 번호·실제 프로토콜 좌표 변환·Gomocup 통신·외부 AI·모의 AI는 미구현이다. 좌표 테스트는 상태 좌표와 선형 인덱스만 검증한다.
- MSVC, 개발 도구 없는 별도 Windows 10·11 환경, 비Windows 빌드는 이번 검증 환경에 없어 실행하지 않았다. 루트 프로젝트는 기존 Windows x64 전용이다.
- DPI·복수 모니터·최대화/복원·장시간 GUI/GDI 자원 사용·오류 대화상자는 추가 검증하지 않았다. 기본 표시·닫기 회귀 확인만 수행했다.
- 스레드 안전성과 기보/스냅샷 직렬화는 제공하지 않는다. 상태는 단일 작성자 계약이며 읽기 전용 span도 자동으로 동기화되지 않는다.
- 실제 매니저·엔진 실행, 대상 대회 규칙 일치와 라이선스 미확정 항목은 기존 후보 조사·승인 기록의 후속 과제로 남긴다.
