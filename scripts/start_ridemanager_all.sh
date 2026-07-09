#!/usr/bin/env bash
set -Eeuo pipefail

# Y.L.M RideManager stack launcher.
# Starts RideManager, imu_service and RideManagerQml from one project root.

START_MAIN="${START_MAIN:-1}"
START_IMU="${START_IMU:-1}"
START_QT="${START_QT:-1}"
BUILD_IF_MISSING="${BUILD_IF_MISSING:-1}"
EXIT_ON_CHILD_EXIT="${EXIT_ON_CHILD_EXIT:-1}"

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
if [[ -d "${SCRIPT_DIR}/../RideManager" && -d "${SCRIPT_DIR}/../imu_project" ]]; then
    PROJECT_ROOT="${PROJECT_ROOT:-$(cd "${SCRIPT_DIR}/.." && pwd)}"
elif [[ -d "${PWD}/RideManager" && -d "${PWD}/imu_project" ]]; then
    PROJECT_ROOT="${PROJECT_ROOT:-${PWD}}"
else
    PROJECT_ROOT="${PROJECT_ROOT:-/Users/yuanyi/Documents/嵌入式比赛/Y_L_M_Project}"
fi

RIDEMANAGER_DIR="${RIDEMANAGER_DIR:-${PROJECT_ROOT}/RideManager}"
IMU_DIR="${IMU_DIR:-${PROJECT_ROOT}/imu_project}"
QML_DIR="${QML_DIR:-${PROJECT_ROOT}/RideManagerQml}"

RIDEMANAGER_CONFIG="${RIDEMANAGER_CONFIG:-${RIDEMANAGER_DIR}/config.toml}"
IMU_CONFIG="${IMU_CONFIG:-${IMU_DIR}/config/config.yaml}"

RUN_STAMP="$(date +%Y%m%d_%H%M%S)"
LOG_DIR="${LOG_DIR:-/tmp/ridemanager_stack_${RUN_STAMP}}"

PIDS=()
NAMES=()

usage() {
    cat <<'EOF'
Usage:
  ./scripts/start_ridemanager_all.sh [options] [-- extra RideManager args]

Options:
  --no-main           Do not start RideManager.
  --no-imu            Do not start imu_service.
  --no-qt             Do not start RideManagerQml.
  --build-if-missing  Build missing native binaries into /tmp (default).
  --keep-going        Do not stop the whole stack when one child exits.
  -h, --help          Show this help.

Environment overrides:
  PROJECT_ROOT, RIDEMANAGER_DIR, IMU_DIR, QML_DIR,
  RIDEMANAGER_CONFIG, IMU_CONFIG, LOG_DIR,
  START_MAIN, START_IMU, START_QT, BUILD_IF_MISSING, EXIT_ON_CHILD_EXIT.

Examples:
  ./scripts/start_ridemanager_all.sh
  ./scripts/start_ridemanager_all.sh --no-qt
  ./scripts/start_ridemanager_all.sh -- --duration 30
EOF
}

MAIN_EXTRA_ARGS=()
while [[ $# -gt 0 ]]; do
    case "$1" in
        --no-main)
            START_MAIN="0"
            ;;
        --no-imu)
            START_IMU="0"
            ;;
        --no-qt)
            START_QT="0"
            ;;
        --build-if-missing)
            BUILD_IF_MISSING="1"
            ;;
        --keep-going)
            EXIT_ON_CHILD_EXIT="0"
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        --)
            shift
            MAIN_EXTRA_ARGS+=("$@")
            break
            ;;
        *)
            MAIN_EXTRA_ARGS+=("$1")
            ;;
    esac
    shift
done

info() {
    printf '[%s] %s\n' "$(date +%H:%M:%S)" "$*"
}

warn() {
    printf '[%s] WARNING: %s\n' "$(date +%H:%M:%S)" "$*" >&2
}

die() {
    printf '[%s] ERROR: %s\n' "$(date +%H:%M:%S)" "$*" >&2
    exit 1
}

require_dir() {
    local dir="$1"
    local label="$2"
    [[ -d "$dir" ]] || die "${label} directory not found: ${dir}"
}

pick_executable() {
    local candidate
    for candidate in "$@"; do
        if [[ -x "$candidate" && ! -d "$candidate" ]]; then
            printf '%s\n' "$candidate"
            return 0
        fi
    done
    return 1
}

cleanup() {
    local code=$?
    trap - INT TERM EXIT

    if ((${#PIDS[@]} > 0)); then
        info "Stopping ${#PIDS[@]} child process(es)..."
        local pid
        for pid in "${PIDS[@]}"; do
            kill -TERM "$pid" 2>/dev/null || true
        done
        sleep 2
        for pid in "${PIDS[@]}"; do
            kill -KILL "$pid" 2>/dev/null || true
        done
        for pid in "${PIDS[@]}"; do
            wait "$pid" 2>/dev/null || true
        done
    fi

    info "Logs kept in: ${LOG_DIR}"
    exit "$code"
}

trap cleanup INT TERM EXIT

start_process() {
    local name="$1"
    local workdir="$2"
    shift 2

    mkdir -p "$LOG_DIR"
    local log_file="${LOG_DIR}/${name}.log"
    info "Starting ${name}"
    info "  workdir: ${workdir}"
    info "  command: $*"
    info "  log: ${log_file}"

    (
        cd "$workdir"
        exec "$@"
    ) >"$log_file" 2>&1 &

    local pid=$!
    PIDS+=("$pid")
    NAMES+=("$name")

    sleep 1
    if ! kill -0 "$pid" 2>/dev/null; then
        set +e
        wait "$pid"
        local status=$?
        set -e
        warn "${name} exited immediately with status ${status}. Last log lines:"
        tail -n 80 "$log_file" >&2 || true
        exit "$status"
    fi
}

check_database_hint() {
    if command -v pg_isready >/dev/null 2>&1; then
        if pg_isready -h "${RIDEMANAGER_DB_HOST:-localhost}" -p "${RIDEMANAGER_DB_PORT:-5432}" >/dev/null 2>&1; then
            info "PostgreSQL is reachable."
        else
            warn "PostgreSQL is not reachable on ${RIDEMANAGER_DB_HOST:-localhost}:${RIDEMANAGER_DB_PORT:-5432}."
            warn "Start PostgreSQL and initialize the database before expecting UI data."
        fi
    else
        warn "pg_isready not found; skipping PostgreSQL readiness check."
    fi
}

check_device_hints() {
    if [[ "$START_MAIN" == "1" ]]; then
        [[ -e /dev/video0 ]] || warn "/dev/video0 not found; camera pipeline may fail on this host."
    fi
    if [[ "$START_IMU" == "1" ]]; then
        [[ -e /dev/i2c-4 ]] || warn "/dev/i2c-4 not found; IMU service may need the target board or i2c permissions."
    fi
}

build_imu_if_needed() {
    local build_dir="${TMPDIR:-/tmp}/ylm_imu_build"
    info "Building imu_service into ${build_dir}" >&2
    cmake -S "$IMU_DIR" -B "$build_dir" -DCMAKE_BUILD_TYPE=Release >&2
    cmake --build "$build_dir" --parallel >&2
    printf '%s\n' "${build_dir}/imu_service"
}

build_qml_if_needed() {
    local build_dir="${TMPDIR:-/tmp}/ylm_qml_build"
    info "Building RideManagerQml into ${build_dir}" >&2
    cmake -S "$QML_DIR" -B "$build_dir" -DCMAKE_BUILD_TYPE=Release >&2
    cmake --build "$build_dir" --parallel >&2

    pick_executable \
        "${build_dir}/RideManagerQml" \
        "${build_dir}/RideManagerQml.app/Contents/MacOS/RideManagerQml"
}

resolve_ridemanager() {
    local uname_s
    local exe
    uname_s="$(uname -s)"

    if [[ "$uname_s" == "Linux" ]]; then
        if exe="$(pick_executable \
            "${RIDEMANAGER_DIR}/bin/Release/net10.0/linux-arm64/publish/RideManager" \
            "${RIDEMANAGER_DIR}/bin/Release/net10.0/linux-arm64/RideManager" \
            "${RIDEMANAGER_DIR}/bin/Debug/net10.0/linux-arm64/RideManager")"; then
            printf '%s\n' "$exe"
            return 0
        fi
    else
        if exe="$(pick_executable \
            "${RIDEMANAGER_DIR}/bin/Release/net10.0/osx-arm64/publish/RideManager" \
            "${RIDEMANAGER_DIR}/bin/Release/net10.0/osx-arm64/RideManager" \
            "${RIDEMANAGER_DIR}/bin/Debug/net10.0/osx-arm64/RideManager")"; then
            printf '%s\n' "$exe"
            return 0
        fi
    fi

    command -v dotnet >/dev/null 2>&1 || die "RideManager executable not found and dotnet is unavailable."
    printf '%s\n' "dotnet-run"
}

resolve_imu() {
    local exe
    if exe="$(pick_executable \
        "${IMU_DIR}/build/imu_service" \
        "${IMU_DIR}/cmake-build-debug/imu_service")"; then
        printf '%s\n' "$exe"
        return 0
    fi

    [[ "$BUILD_IF_MISSING" == "1" ]] || die "imu_service executable not found. Re-run with --build-if-missing."
    build_imu_if_needed
}

resolve_qml() {
    local exe
    if exe="$(pick_executable \
        "${QML_DIR}/build/RideManagerQml" \
        "${QML_DIR}/build/RideManagerQml.app/Contents/MacOS/RideManagerQml" \
        "${QML_DIR}/cmake-build-debug/RideManagerQml" \
        "${QML_DIR}/cmake-build-debug/RideManagerQml.app/Contents/MacOS/RideManagerQml")"; then
        printf '%s\n' "$exe"
        return 0
    fi

    [[ "$BUILD_IF_MISSING" == "1" ]] || die "RideManagerQml executable not found. Re-run with --build-if-missing."
    build_qml_if_needed
}

start_main() {
    require_dir "$RIDEMANAGER_DIR" "RideManager"
    [[ -f "$RIDEMANAGER_CONFIG" ]] || die "RideManager config not found: ${RIDEMANAGER_CONFIG}"

    local exe
    exe="$(resolve_ridemanager)"
    if [[ "$exe" == "dotnet-run" ]]; then
        start_process "ridemanager" "$RIDEMANAGER_DIR" \
            dotnet run --project "$RIDEMANAGER_DIR/RideManager.csproj" -- \
            --config "$RIDEMANAGER_CONFIG" "${MAIN_EXTRA_ARGS[@]}"
    else
        start_process "ridemanager" "$RIDEMANAGER_DIR" \
            "$exe" --config "$RIDEMANAGER_CONFIG" "${MAIN_EXTRA_ARGS[@]}"
    fi
}

start_imu() {
    require_dir "$IMU_DIR" "IMU"
    [[ -f "$IMU_CONFIG" ]] || die "IMU config not found: ${IMU_CONFIG}"

    local exe
    exe="$(resolve_imu)"
    start_process "imu" "$IMU_DIR" "$exe" --config "$IMU_CONFIG"
}

start_qt() {
    require_dir "$QML_DIR" "RideManagerQml"
    export RIDEMANAGER_CONFIG_PATH="$RIDEMANAGER_CONFIG"

    local exe
    exe="$(resolve_qml)"
    start_process "qt" "$QML_DIR" "$exe"
}

main() {
    mkdir -p "$LOG_DIR"
    info "Y.L.M RideManager stack launcher"
    info "Project root: ${PROJECT_ROOT}"
    info "Logs: ${LOG_DIR}"

    check_database_hint
    check_device_hints

    if [[ "$START_MAIN" == "1" ]]; then
        start_main
    else
        info "Skipping RideManager."
    fi

    if [[ "$START_IMU" == "1" ]]; then
        start_imu
    else
        info "Skipping IMU."
    fi

    if [[ "$START_QT" == "1" ]]; then
        start_qt
    else
        info "Skipping Qt UI."
    fi

    info "All requested processes started. Press Ctrl-C to stop."

    while true; do
        sleep 2
        local index
        for index in "${!PIDS[@]}"; do
            local pid="${PIDS[$index]}"
            local name="${NAMES[$index]}"
            if ! kill -0 "$pid" 2>/dev/null; then
                set +e
                wait "$pid"
                local status=$?
                set -e
                warn "${name} exited with status ${status}."
                warn "Log file: ${LOG_DIR}/${name}.log"
                if [[ "$EXIT_ON_CHILD_EXIT" == "1" ]]; then
                    exit "$status"
                fi
                unset 'PIDS[index]'
                unset 'NAMES[index]'
            fi
        done
    done
}

main
