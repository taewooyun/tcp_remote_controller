# tcp_remote_controller
tcp 원격 장치 제어 프로그램



## Build System

- CMake 기반 빌드 시스템 구성
- 센서 제어 모듈을 공유 라이브러리(.so)로 분리
- client / server 실행 파일 분리 빌드
- rpath 설정을 통해 별도 환경 변수 없이 실행 가능