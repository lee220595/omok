# Rapfi 초기 연결 실제 검증

2026-10-24. 작업 기준은 `e6cbcb6619debf9c40361f5582bd4caf85f64b33`, 브랜치는 `codex/rapfi-integration`이다. 시작 변경은 사용자 설치 external/뿐이었으며 기존 코드 미커밋 변경은 없었다. fetch 후 기존 작업 브랜치와 원격 일치를 확인하고 미병합 GUI 이력을 보존해 새 브랜치를 만들었다. 기존 main으로 되돌려 코드를 잃지 않았다. 최종 커밋/푸시는 완료 보고와 Git에서 확인한다.

## 설치 확인과 공식 근거

[설치 파일 전체 크기·SHA-256](verification/rapfi-installation.json)을 기록했다. 선택한 파일은 `external/rapfi/Rapfi-engine/pbrain-rapfi-windows-avx2.exe`, 해시는 `1E02497433FB765626F954C895B0B92E057AC12A76C76318ABE74D4050914E8E`다. ABOUT는 Rapfi0.43.01 / clang++19.1.5 / Windows SSE41 AVX2다. config 해시 `32B9180C91247541D399CEEF62906D5FFEB48E34EB5F261612987633820D5D3C`는 작업 후에도 같았다. 가중치는 읽기만 했으며 변경하지 않았다.

config는 최소0.43.1, mix9svq, coord_conversion_mode none, thread1, DB 비활성, memory_reserved_mb0이다. 실제 메시지에서 렌주 흑/백 가중치 둘 다 로드한 것을 확인했다. classical model210901.bin도 원본과 함께 보존한다. Linux/macOS/AVX512/VNNI 실행 파일은 실행하지 않았다.

CIM CPU 조회는 제한 환경에서 실패해 .NET 지원 검사와 자체 CPUID/OSXSAVE/XGETBV 검사로 확인했다. CPU는 Intel Core Ultra5 226V, SSE41/AVX2/FMA/BMI2/AVXVNNI 지원, AVX512 미지원이다. 실행 안전성을 우선해 AVX2를 선택했다.

공식 근거는 [프로토콜](https://plastovicka.github.io/protocl2en.htm), [Rapfi 릴리스](https://github.com/dotb1ue/rapfi/releases/tag/250615), [고정 gomocup 소스](https://github.com/dotb1ue/rapfi/blob/1be1551ced57e38d53ed58f6d74bf6f8b4bdc230/Rapfi/command/gomocup.cpp), [설정/배치 안내·GPL](https://github.com/dotb1ue/rapfi/blob/1be1551ced57e38d53ed58f6d74bf6f8b4bdc230/Readme.md)다. 해당 소스는 명령/색 처리 조사 기준이며 로컬 바이너리와 정확한 소스 빌드 대응은 미증명이다. 사용자 다운로드 ZIP의 원본 이름/해시도 확보하지 않았다. Rapfi나 모델을 재배포하지 않는다.

## 실제 명령·최종 결과

기존 Windows x64 호스트, GCC13.3.0/MinGW-w64 11.0.1, CMake3.29.3, Ninja1.12.1에서 실행했다. 스크립트와 ASCII junction 우회는 유지했다.

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -AliasParent "$env:TEMP\omok-rules-build"
ctest --test-dir .\build\native-release --output-on-failure --no-tests=error
.\build\native-release\bin\omok_engine_probe.exe .\external\rapfi\Rapfi-engine\pbrain-rapfi-windows-avx2.exe 1000
Get-FileHash build/native-release/bin/omok.exe,dist/omok/omok.exe -Algorithm SHA256
& 'C:/Program Files/mingw64/bin/objdump.exe' -p .\dist\omok\omok.exe | Select-String 'DLL Name'
Get-Process -Name omok,'pbrain-rapfi*' -ErrorAction SilentlyContinue
```

- 최종 configure·35개 컴파일/링크·install 모두0, 컴파일 경고 출력 없음.
- **최종 CTest86/86 통과**, 9.15초. 상태14·규칙39·오프닝14·match14·AI5. 모의 AI는 고정 응답 fixture이며 실력 있는 AI로 소개하지 않는다. 최종 코드 확인 후 실행 코드는 추가 변경하지 않았다.
- 모의 테스트: 순서/좌표/자신-상대·분할 라인/CR·진단/stderr·정상 적용·중복/범위/금수·잘못된 출력/긴 라인·시작 거부/초기화 중 종료·응답 전/후 종료·응답 시간 초과·END를 받지 않는 프로세스의 제한 정리·취소/새 세션/종료 후 응답 거부. 실제 자식 PID가 종료됐는지도 확인했다.
- build/dist GUI 실행 파일 해시 일치: `1132A9192ECBF9DCC74599CF8E0A1A0DFC2D0BC90B2DC679ED8E27CDC7DDEB35`.
- GUI import는 GDI32/KERNEL32/msvcrt/USER32뿐이며 선택 Rapfi는 KERNEL32/ADVAPI32다. 개발 도구 없는 별도 Windows10/11 실행까지 확인한 것은 아니다.
- 주요 문서의 로컬 링크101개를 확인해 누락0, git diff --check 통과. 설치 폴더18개 원본 파일의 해시를 다시 대조해 변경0을 확인했다. 실행용 README/고지의 source와 dist 해시도 일치했다.

### 실제 Rapfi 프로브

[최종 출력](verification/rapfi-probe.txt), [case0 통신](verification/rapfi-case-0.txt), [case1 통신](verification/rapfi-case-1.txt), [case2 통신](verification/rapfi-case-2.txt)을 보존했다. 로그는 실제 엔진에서 얻었으며 예상값으로 만든 출력이 아니다.

| 경우 | 준비 배치·주체 | 응답·적용 | 전체 요청 시간 |
|---|---|---|---:|
| 0 | h8i8g9, 흑A/백B, 엔진B | 8,6 → 상태{6,8}, 백 I7. match 합법 적용 | 947.307ms |
| 1 | 같은3수 후 사람이 백 I9, 엔진A 흑 | 8,6 → 흑 I7. match 합법 적용 | 918.763ms |
| 2 | 같은3수, 흑B/백A, 엔진A 백 | 8,6 → 백 I7. A/B 변경 후에도 올바른 매핑 | 922.211ms |

모두 초기화/응답 성공, 원본 요청 상태 보존, apply_result에서 기존 rules/Controller 적용, PID 종료·reaped1·forced0·exit0를 확인했다. 프로브1000ms는 검증용 한 수 예산으로 GUI30초 기본을 바꾸지 않는다. 위 시간은 초기화와 정리를 포함하며 한 수 deadline 계측 범위와 구분한다.

### 실제 GUI 관찰

Computer Use의 sky API로 프로젝트 창만 조작했다. 처음 coordinate geometry unavailable/다른 창 캡처를 만났으나 대상 활성화·재관찰 후 진행했다. 이번에는 사용자 중단 신호를 받지 않았다.

| 동작 | 관찰 결과 |
|---|---|
| 모드/경로 | 실제 AVX2 경로 자동 채움, 사람A/RapfiB와 RapfiA/사람B 선택 가능 |
| 백 Rapfi 시작 | 개발3수·흑A/백B로 시작, 백 I7이4수로 적용되고 흑A 차례/마지막 수 표시 |
| 사람 입력 | 화면 H7을 실제 클릭, 흑A의5수로 확정하고 다음 Rapfi 요청 시작 |
| 엔진 대기 중 재시작 | 원래3수/색 배정/180초 예산 복원. 새 세션 요청과 이전 요청을 분리 |
| 재시작 후 기권 | 사람 흑 기권으로 백B 승리·3수 유지. 취소된 요청을 적용하지 않음 |
| 최종 흑 Rapfi 경로 | RapfiA/사람B로 시작해 백 I9 클릭, 흑 I7 응답이5수로 적용됨. 사람 기권 후 흑A 승리 |
| 응답/종료 표시 | 긴 ABOUT/전체 로그 경로를 줄이고 남은 시간 표시를 위로 배치. 종료 후 사고 중 문구 제거를 확인 |
| 요청 중 닫기 | 재시작·백I9 후 Alt+F4. 실제 요청 취소 로그(reaped1/forced0/exit0), 창·omok/Rapfi PID 소멸 확인 |
| 반응성 | 엔진 요청 중 재시작·기권·닫기 동작에 GUI가 응답함. 지연 상한/장시간 반응성 보증은 아님 |

GUI 로그의 정상 백 응답은11442.5ms, 다음 백 응답3977.97ms, 흑 응답4154.66ms였다. 취소된 요청19164는4826.97ms, 닫기 때 요청21240은4563.5ms로 모두 정상 정리됐다. 이 값은 사고 시작부터 정리까지 전체 시간이며 창 닫기 자체의 지연이 아니다. 원본 전체 로그는 로컬 dist/omok/logs에 있고 선별한 취소/닫기 통신은 아래 파일에 저장했다. 종료 정리 END는 현재 raw transcript에 별도 프레임으로 기록하지 않고 최종 reaped/forced/exit 요약으로 확인한다.

[기권 후 취소 통신](verification/rapfi-gui-cancel.txt) · [창 닫기 취소 통신](verification/rapfi-gui-close.txt).

Git에 보존한 통신 텍스트는 빈 수신 줄의 표시 접두사 뒤 공백만 제거했다(`< ` → `<`). 명령·응답 값·결과는 변경하지 않았으며 원시 .log는 로컬에 보존한다.

마지막 사람 차례로 재시작했을 때 이전 종료 안내가 남는 문구를 한 줄 보완했고 빌드/전체 테스트를 다시 했다. 그 마지막 문구만의 시각 재확인은 미실행이다. 엔진 정상 응답 경로는 최종 C++ 프로브로 다시 확인했다.

## 실패에서 수정한 점

1. ai 디렉터리에서 Threads::Threads 가져오기 범위 누락으로 첫 CMake generate 실패. 해당 범위에서 find_package를 호출해 해결했다.
2. 최초 C++ 실제 시험에서 INFO max_memory1GiB가 TT1GiB 할당을 유도해 hard1GiB Job에서 ERROR가 났다. 전체 제한을 유지하고 엔진 예산512MiB로 줄여 모델/런타임 여유를 확보했다. 성공으로 덮어쓰지 않는다.
3. 가중치 지연 로드가 Rapfi 검색 시계 밖에 있어1초 시험에서 관리자 시간 초과를 관찰했다. INFO 처리 확인과 시간 차감·검색 여유를 추가하고 관리자 deadline은 완화하지 않았다. 최종 세 경우 모두 성공했다.
4. GUI의 긴 진단 표시·기권 후 사고 중 표시·재시작 후 이전 종료 표시를 보완했다. 기권 후 종료 문구는 실제 확인, 마지막 재시작 문구는 빌드/코드 확인만 했다.

## 남은 범위·다음 검증

- 매 요청 새 프로세스/가중치 로드로 캐시 미재사용과 시작 비용이 있다. 실제 강도/긴 대국 성능을 비교하지 않았다. 초기 연결의 기술 선택이며 최적화 완료가 아니다.
- 실제 Rapfi와 승리/금수/200수까지 전체 대국, 실제 오류 주입·누적180초 소진·메모리 경계/피크·다중 모니터DPI·장시간 누수는 미검증이다. 오류·종료 경계의 현재 자동 증거는 모의 엔진이다.
- 한 논리CPU/Job메모리/INFO 제약 적용 성공은 확인했지만 사람 시계·전체 대회 자원 정책·실제 대회 서버/매니저 준수는 미완료다.
- AI 대 AI, 증분 BEGIN/TURN, SWAP2BOARD/Swap2 규칙 바인딩, 공식 준비 데이터, Piskvork/전체GomocupJudge 운영, 기보, 자체AI는 후속이다. 기본 준비 렌주를 다른 규칙으로 바꾸지 않았다.
- 이전 로컬 대국 단계의 모든 수동 항목이 완료된 것은 아니다. 다만 실제 시작·사람 클릭·흑/백 AI 응답·재시작·기권·닫기 증거는 이번에 추가됐다. 전체5/6단계나 대회 적합성 완료로 확대하지 않는다.
- 외부 실행 파일/모델은 Git/install에 포함하지 않는다. 소스/버전/모델별 재배포 조건 확인 없이 ZIP에 추가하지 않는다. 목표 연도/리그도 여전히 미정이다.
