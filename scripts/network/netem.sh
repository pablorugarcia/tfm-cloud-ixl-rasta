#!/usr/bin/env bash

set -Eeuo pipefail

readonly DEFAULT_OC_IP="192.168.0.152"
readonly ROOT_HANDLE="c1d1:"
readonly DEFAULT_NETEM_HANDLE="c1d2:"
readonly IMPAIRED_CLASS="c1d1:1"

OC_IP="${OC_IP:-$DEFAULT_OC_IP}"
OC_CH1_IP="${OC_CH1_IP:-$OC_IP}"
OC_CH1_PORT="${OC_CH1_PORT:-8888}"
OC_CH2_IP="${OC_CH2_IP:-$OC_IP}"
OC_CH2_PORT="${OC_CH2_PORT:-8889}"
NETEM_IFACE="${NETEM_IFACE:-}"
REORDER_DELAY_MS="${REORDER_DELAY_MS:-100}"
NETEM_HANDLE="$DEFAULT_NETEM_HANDLE"

APPLIED_IFACE=""
APPLIED_NETEM_HANDLE=""
APPLY_IN_PROGRESS=0
TIMED_OUTAGE=0
declare -a OWNED_INTERFACES=()

usage() {
    cat <<'EOF'
Usage:
  sudo ./scripts/network/netem.sh status
  sudo ./scripts/network/netem.sh clear
  sudo ./scripts/network/netem.sh delay MILLISECONDS
  sudo ./scripts/network/netem.sh loss PERCENT
  sudo ./scripts/network/netem.sh duplicate PERCENT
  sudo ./scripts/network/netem.sh reorder PERCENT
  sudo ./scripts/network/netem.sh outage [SECONDS]
  sudo ./scripts/network/netem.sh channel1-loss
  sudo ./scripts/network/netem.sh channel2-loss

The impairment is limited to IPv4/UDP egress from Cloud IXL to the OC.

Environment variables:
  OC_IP              common OC IP (default: 192.168.0.152)
  OC_CH1_IP          channel 1 OC IP (default: OC_IP)
  OC_CH1_PORT        channel 1 OC UDP port (default: 8888)
  OC_CH2_IP          channel 2 OC IP (default: OC_IP)
  OC_CH2_PORT        channel 2 OC UDP port (default: 8889)
  NETEM_IFACE        interface to use; otherwise detected with ip route get
  REORDER_DELAY_MS   auxiliary delay for reorder (default: 100)

Pass variables through sudo with, for example:
  sudo env OC_IP=127.0.0.1 NETEM_IFACE=lo ./scripts/network/netem.sh delay 100
EOF
}

die() {
    printf 'Error: %s\n' "$*" >&2
    exit 1
}

require_command() {
    command -v "$1" >/dev/null 2>&1 || die "required command '$1' was not found"
}

require_tools() {
    require_command awk
    require_command ip
    require_command tc
}

require_root() {
    (( EUID == 0 )) || die "this command changes network state; run it with sudo"
}

validate_integer_range() {
    local value="$1"
    local label="$2"
    local minimum="$3"
    local maximum="$4"
    local number

    [[ "$value" =~ ^[0-9]{1,10}$ ]] || \
        die "$label must be an integer between $minimum and $maximum"
    number=$((10#$value))
    (( number >= minimum && number <= maximum )) || \
        die "$label must be between $minimum and $maximum"
}

valid_ipv4() {
    local value="$1"
    local octet
    local -a octets

    [[ "$value" =~ ^([0-9]{1,3}\.){3}[0-9]{1,3}$ ]] || return 1
    IFS='.' read -r -a octets <<<"$value"
    (( ${#octets[@]} == 4 )) || return 1

    for octet in "${octets[@]}"; do
        [[ "$octet" =~ ^[0-9]{1,3}$ ]] || return 1
        [[ ${#octet} -eq 1 || "$octet" != 0* ]] || return 1
        (( 10#$octet <= 255 )) || return 1
    done
}

validate_configuration() {
    valid_ipv4 "$OC_CH1_IP" || die "OC_CH1_IP is not a valid IPv4 address: $OC_CH1_IP"
    valid_ipv4 "$OC_CH2_IP" || die "OC_CH2_IP is not a valid IPv4 address: $OC_CH2_IP"
    [[ "$OC_CH1_IP" != "0.0.0.0" ]] || die "OC_CH1_IP cannot be the wildcard address 0.0.0.0"
    [[ "$OC_CH2_IP" != "0.0.0.0" ]] || die "OC_CH2_IP cannot be the wildcard address 0.0.0.0"
    validate_integer_range "$OC_CH1_PORT" "OC_CH1_PORT" 1 65535
    validate_integer_range "$OC_CH2_PORT" "OC_CH2_PORT" 1 65535

    if [[ -n "$NETEM_IFACE" ]]; then
        [[ ${#NETEM_IFACE} -le 15 && "$NETEM_IFACE" =~ ^[[:alnum:]_.:-]+$ ]] || \
            die "NETEM_IFACE is not a valid Linux interface name: $NETEM_IFACE"
    fi
}

require_distinct_channels() {
    [[ "$OC_CH1_IP:$OC_CH1_PORT" != "$OC_CH2_IP:$OC_CH2_PORT" ]] || \
        die "the two channels have the same destination endpoint and cannot be isolated"
}

validate_interface() {
    ip link show dev "$1" >/dev/null 2>&1 || die "network interface '$1' does not exist"
}

route_interface() {
    local destination="$1"
    local route
    local interface

    route=$(ip -4 route get "$destination" 2>&1) || \
        die "no IPv4 route to $destination: $route"
    interface=$(awk '{ for (i = 1; i <= NF; i++) if ($i == "dev") { print $(i + 1); exit } }' <<<"$route")
    [[ -n "$interface" ]] || die "could not determine the interface from: $route"
    printf '%s\n' "$interface"
}

verify_route() {
    local destination="$1"
    local expected_interface="$2"
    local label="$3"
    local actual_interface

    actual_interface=$(route_interface "$destination")
    [[ "$actual_interface" == "$expected_interface" ]] || \
        die "$label ($destination) uses '$actual_interface', not '$expected_interface'; one invocation only manages one egress interface"
}

resolve_interface() {
    local scope="$1"
    local interface

    if [[ -n "$NETEM_IFACE" ]]; then
        interface="$NETEM_IFACE"
    elif [[ "$scope" == "channel2" ]]; then
        interface=$(route_interface "$OC_CH2_IP")
    else
        interface=$(route_interface "$OC_CH1_IP")
    fi

    validate_interface "$interface"

    case "$scope" in
        both)
            verify_route "$OC_CH1_IP" "$interface" "OC channel 1"
            verify_route "$OC_CH2_IP" "$interface" "OC channel 2"
            ;;
        channel1)
            verify_route "$OC_CH1_IP" "$interface" "OC channel 1"
            ;;
        channel2)
            verify_route "$OC_CH2_IP" "$interface" "OC channel 2"
            ;;
        *)
            die "internal error: unknown traffic scope '$scope'"
            ;;
    esac

    printf '%s\n' "$interface"
}

list_interfaces() {
    ip -o link show | awk -F': ' '{ name = $2; sub(/@.*/, "", name); print name }'
}

interface_has_owned_qdisc() {
    local output

    output=$(tc qdisc show dev "$1" 2>&1) || \
        die "could not inspect qdiscs on '$1': $output"
    [[ "$output" == *"qdisc prio ${ROOT_HANDLE} root"* ]]
}

collect_owned_interfaces() {
    local interface
    local interface_output

    OWNED_INTERFACES=()
    interface_output=$(list_interfaces 2>&1) || \
        die "could not enumerate network interfaces: $interface_output"

    while IFS= read -r interface; do
        if [[ -n "$interface" ]] && interface_has_owned_qdisc "$interface"; then
            OWNED_INTERFACES+=("$interface")
        fi
    done <<<"$interface_output"
}

mq_tree_is_unmodified_default() {
    local interface="$1"
    local qdisc_output="$2"
    local filter_output
    local line
    local parent
    local -a parents=()

    while IFS= read -r line; do
        [[ -n "$line" ]] || continue

        if [[ "$line" =~ ^qdisc[[:space:]]+mq[[:space:]]+0:[[:space:]]+root([[:space:]]|$) ]]; then
            continue
        fi

        if [[ "$line" =~ ^qdisc[[:space:]]+(pfifo_fast|fq_codel|fq)[[:space:]]+0:[[:space:]]+parent[[:space:]]+(:[[:xdigit:]]+)([[:space:]]|$) ]]; then
            parents+=("${BASH_REMATCH[2]}")
            continue
        fi

        return 1
    done <<<"$qdisc_output"

    (( ${#parents[@]} > 0 )) || return 1

    filter_output=$(tc filter show dev "$interface" 2>&1) || return 1
    [[ -z "${filter_output//[[:space:]]/}" ]] || return 1

    for parent in "${parents[@]}"; do
        filter_output=$(tc filter show dev "$interface" parent "$parent" 2>&1) || return 1
        [[ -z "${filter_output//[[:space:]]/}" ]] || return 1
    done
}

root_is_replaceable_default() {
    local interface="$1"
    local qdisc_output
    local root_line
    local nonempty_lines
    local filter_output

    qdisc_output=$(tc qdisc show dev "$interface" 2>&1) || \
        die "could not inspect qdiscs on '$interface': $qdisc_output"
    root_line=$(awk '/ root([[:space:]]|$)/ { print; exit }' <<<"$qdisc_output")

    if [[ "$root_line" =~ ^qdisc[[:space:]]+mq[[:space:]]+0:[[:space:]]+root([[:space:]]|$) ]]; then
        mq_tree_is_unmodified_default "$interface" "$qdisc_output"
        return
    fi

    case "$root_line" in
        ""|qdisc\ noqueue\ 0:\ root*|qdisc\ pfifo_fast\ 0:\ root*|qdisc\ fq_codel\ 0:\ root*|qdisc\ fq\ 0:\ root*)
            nonempty_lines=$(awk 'NF { count++ } END { print count + 0 }' <<<"$qdisc_output")
            (( nonempty_lines <= 1 )) || return 1
            filter_output=$(tc filter show dev "$interface" 2>&1) || return 1
            [[ -z "${filter_output//[[:space:]]/}" ]]
            ;;
        *)
            return 1
            ;;
    esac
}

remove_owned_qdisc() {
    local interface="$1"

    interface_has_owned_qdisc "$interface" || return 0
    tc qdisc del dev "$interface" root handle "$ROOT_HANDLE"
}

show_tc_state() {
    local interface="$1"

    printf '\ntc -s qdisc show dev %s\n' "$interface"
    tc -s qdisc show dev "$interface"

    if interface_has_owned_qdisc "$interface"; then
        printf '\ntc filter show dev %s parent %s\n' "$interface" "$ROOT_HANDLE"
        tc filter show dev "$interface" parent "$ROOT_HANDLE"
    fi
}

clear_all() {
    local interface
    local cleared=0
    local -a interfaces

    if [[ -n "$NETEM_IFACE" ]]; then
        validate_interface "$NETEM_IFACE"
        interfaces=("$NETEM_IFACE")
    else
        collect_owned_interfaces
        interfaces=("${OWNED_INTERFACES[@]}")
    fi

    for interface in "${interfaces[@]}"; do
        if [[ -n "$interface" ]] && interface_has_owned_qdisc "$interface"; then
            printf 'Removing Cloud IXL network impairment\n'
            printf 'Interface: %s\n' "$interface"
            remove_owned_qdisc "$interface"
            printf 'Network impairment cleared\n'
            show_tc_state "$interface"
            cleared=1
        fi
    done

    if (( cleared == 0 )); then
        printf 'No Cloud IXL network impairment is active; nothing to clear.\n'
    fi
}

show_status() {
    local channel1_interface
    local channel2_interface
    local interface
    local -a inspected_interfaces

    channel1_interface=$(route_interface "$OC_CH1_IP")
    channel2_interface=$(route_interface "$OC_CH2_IP")
    validate_interface "$channel1_interface"
    validate_interface "$channel2_interface"
    collect_owned_interfaces

    printf 'Cloud IXL network impairment status\n'
    printf 'OC channel 1: %s:%s/udp via %s\n' "$OC_CH1_IP" "$OC_CH1_PORT" "$channel1_interface"
    printf 'OC channel 2: %s:%s/udp via %s\n' "$OC_CH2_IP" "$OC_CH2_PORT" "$channel2_interface"
    printf 'Direction: Cloud IXL -> OC (IPv4/UDP egress)\n'

    if [[ -n "$NETEM_IFACE" ]]; then
        validate_interface "$NETEM_IFACE"
        printf 'Requested NETEM_IFACE: %s\n' "$NETEM_IFACE"
    fi

    if (( ${#OWNED_INTERFACES[@]} == 0 )); then
        printf 'State: nominal (no qdisc owned by this script)\n'
        inspected_interfaces=("$channel1_interface")
        if [[ "$channel2_interface" != "$channel1_interface" ]]; then
            inspected_interfaces+=("$channel2_interface")
        fi

        for interface in "${inspected_interfaces[@]}"; do
            if root_is_replaceable_default "$interface"; then
                printf 'Root qdisc check on %s: recognised kernel default with no tc filters\n' "$interface"
            else
                printf 'Root qdisc check on %s: custom or unrecognised; apply commands will refuse it\n' "$interface"
            fi
            show_tc_state "$interface"
        done
        return
    fi

    for interface in "${OWNED_INTERFACES[@]}"; do
        printf 'State: impairment active on %s\n' "$interface"
        show_tc_state "$interface"
    done
}

prepare_interface() {
    local target_interface="$1"
    local interface
    local qdisc_output

    collect_owned_interfaces
    for interface in "${OWNED_INTERFACES[@]}"; do
        if [[ "$interface" != "$target_interface" ]]; then
            die "an impairment owned by this script is active on '$interface'; run clear first"
        fi
    done

    if interface_has_owned_qdisc "$target_interface"; then
        printf 'Replacing the existing Cloud IXL impairment on %s.\n' "$target_interface"
        remove_owned_qdisc "$target_interface"
    fi

    if ! root_is_replaceable_default "$target_interface"; then
        qdisc_output=$(tc qdisc show dev "$target_interface" 2>&1) || \
            die "could not inspect qdiscs on '$target_interface': $qdisc_output"
        die "refusing to replace a root qdisc with unrecognised leaves or filters on '$target_interface': $qdisc_output"
    fi
}

add_endpoint_filter() {
    local interface="$1"
    local destination_ip="$2"
    local destination_port="$3"

    tc filter add dev "$interface" protocol ip parent "$ROOT_HANDLE" prio 1 u32 \
        match ip protocol 17 0xff \
        match ip dst "$destination_ip/32" \
        match ip dport "$destination_port" 0xffff \
        flowid "$IMPAIRED_CLASS"
}

rollback_apply() {
    local interface="$1"

    if interface_has_owned_qdisc "$interface"; then
        printf 'Application failed; removing the partial qdisc from %s.\n' "$interface" >&2
        remove_owned_qdisc "$interface" || \
            printf 'Warning: automatic cleanup failed; run clear with NETEM_IFACE=%s.\n' "$interface" >&2
    fi
}

print_request() {
    local interface="$1"
    local scope="$2"
    local scenario="$3"
    local value="$4"

    printf 'Network impairment request\n'
    printf 'Interface: %s\n' "$interface"
    printf 'Scenario: %s\n' "$scenario"
    printf 'Value: %s\n' "$value"
    printf 'Direction: Cloud IXL -> OC (IPv4/UDP egress)\n'

    case "$scope" in
        both)
            printf 'Matched endpoints: %s:%s/udp, %s:%s/udp\n' \
                "$OC_CH1_IP" "$OC_CH1_PORT" "$OC_CH2_IP" "$OC_CH2_PORT"
            ;;
        channel1)
            printf 'Matched endpoint: channel 1, %s:%s/udp\n' "$OC_CH1_IP" "$OC_CH1_PORT"
            ;;
        channel2)
            printf 'Matched endpoint: channel 2, %s:%s/udp\n' "$OC_CH2_IP" "$OC_CH2_PORT"
            ;;
    esac
}

apply_impairment() {
    local scope="$1"
    local scenario="$2"
    local value="$3"
    local interface
    shift 3
    local -a netem_options=("$@")

    interface=$(resolve_interface "$scope")
    prepare_interface "$interface"
    print_request "$interface" "$scope" "$scenario" "$value"
    APPLIED_IFACE="$interface"
    APPLIED_NETEM_HANDLE="$NETEM_HANDLE"
    APPLY_IN_PROGRESS=1
    trap exit_cleanup EXIT
    trap 'exit 130' INT
    trap 'exit 143' TERM

    if ! tc qdisc replace dev "$interface" root handle "$ROOT_HANDLE" prio bands 2 \
        priomap 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1; then
        rollback_apply "$interface"
        APPLY_IN_PROGRESS=0
        die "could not install the root prio qdisc"
    fi

    if ! tc qdisc replace dev "$interface" parent "$IMPAIRED_CLASS" handle "$NETEM_HANDLE" \
        netem "${netem_options[@]}"; then
        rollback_apply "$interface"
        APPLY_IN_PROGRESS=0
        die "could not install the netem qdisc"
    fi

    case "$scope" in
        both)
            if ! add_endpoint_filter "$interface" "$OC_CH1_IP" "$OC_CH1_PORT"; then
                rollback_apply "$interface"
                APPLY_IN_PROGRESS=0
                die "could not install the channel 1 traffic filter"
            fi
            if [[ "$OC_CH1_IP:$OC_CH1_PORT" != "$OC_CH2_IP:$OC_CH2_PORT" ]] && \
                ! add_endpoint_filter "$interface" "$OC_CH2_IP" "$OC_CH2_PORT"; then
                rollback_apply "$interface"
                APPLY_IN_PROGRESS=0
                die "could not install the channel 2 traffic filter"
            fi
            ;;
        channel1)
            if ! add_endpoint_filter "$interface" "$OC_CH1_IP" "$OC_CH1_PORT"; then
                rollback_apply "$interface"
                APPLY_IN_PROGRESS=0
                die "could not install the channel 1 traffic filter"
            fi
            ;;
        channel2)
            if ! add_endpoint_filter "$interface" "$OC_CH2_IP" "$OC_CH2_PORT"; then
                rollback_apply "$interface"
                APPLY_IN_PROGRESS=0
                die "could not install the channel 2 traffic filter"
            fi
            ;;
    esac

    printf '\nNetwork impairment applied\n'
    printf 'Interface: %s\n' "$interface"
    printf 'Scenario: %s\n' "$scenario"
    printf 'Value: %s\n' "$value"
    show_tc_state "$interface"
    APPLY_IN_PROGRESS=0
}

current_operation_is_active() {
    local output

    [[ -n "$APPLIED_IFACE" && -n "$APPLIED_NETEM_HANDLE" ]] || return 1
    if ! output=$(tc qdisc show dev "$APPLIED_IFACE" 2>&1); then
        printf 'Warning: could not inspect qdiscs on %s during timed cleanup: %s\n' \
            "$APPLIED_IFACE" "$output" >&2
        return 2
    fi
    [[ "$output" == *"qdisc prio ${ROOT_HANDLE} root"* && \
       "$output" == *"qdisc netem ${APPLIED_NETEM_HANDLE} parent ${IMPAIRED_CLASS}"* ]]
}

timed_netem_handle() {
    printf '%x:\n' $((0x2000 + (BASHPID % 0x6000)))
}

partial_apply_cleanup() {
    local output

    [[ -n "$APPLIED_IFACE" ]] || return 0
    if ! output=$(tc qdisc show dev "$APPLIED_IFACE" 2>&1); then
        printf 'Warning: could not inspect partial qdisc on %s: %s\n' \
            "$APPLIED_IFACE" "$output" >&2
        return 0
    fi

    if [[ "$output" == *"qdisc prio ${ROOT_HANDLE} root"* ]]; then
        printf '\nInterrupted application cleanup\n' >&2
        printf 'Interface: %s\n' "$APPLIED_IFACE" >&2
        tc qdisc del dev "$APPLIED_IFACE" root handle "$ROOT_HANDLE" || \
            printf 'Warning: partial qdisc cleanup failed; run clear with NETEM_IFACE=%s.\n' \
                "$APPLIED_IFACE" >&2
    fi
}

timed_cleanup() {
    local operation_status=0

    current_operation_is_active || operation_status=$?
    (( operation_status == 0 )) || return 0

    printf '\nTimed outage cleanup\n'
    printf 'Interface: %s\n' "$APPLIED_IFACE"
    remove_owned_qdisc "$APPLIED_IFACE" || true
}

exit_cleanup() {
    if (( APPLY_IN_PROGRESS )); then
        partial_apply_cleanup
    elif (( TIMED_OUTAGE )); then
        timed_cleanup
    fi
}

expect_argument_count() {
    local actual="$1"
    local expected="$2"
    local command="$3"

    (( actual == expected )) || die "'$command' expects $expected argument(s); use --help"
}

main() {
    local command="${1:---help}"
    local operation_status
    local value

    case "$command" in
        -h|--help|help)
            usage
            return
            ;;
    esac

    require_tools

    case "$command" in
        status)
            expect_argument_count "$#" 1 "$command"
            validate_configuration
            show_status
            ;;
        clear)
            expect_argument_count "$#" 1 "$command"
            require_root
            clear_all
            ;;
        delay)
            expect_argument_count "$#" 2 "$command"
            value="$2"
            validate_integer_range "$value" "delay" 1 600000
            validate_configuration
            require_root
            apply_impairment both delay "$value ms" delay "${value}ms"
            ;;
        loss)
            expect_argument_count "$#" 2 "$command"
            value="$2"
            validate_integer_range "$value" "loss" 1 100
            validate_configuration
            require_root
            apply_impairment both loss "$value %" loss "${value}%"
            ;;
        duplicate)
            expect_argument_count "$#" 2 "$command"
            value="$2"
            validate_integer_range "$value" "duplicate" 1 100
            validate_configuration
            require_root
            apply_impairment both duplicate "$value %" duplicate "${value}%"
            ;;
        reorder)
            expect_argument_count "$#" 2 "$command"
            value="$2"
            validate_integer_range "$value" "reorder" 1 99
            validate_integer_range "$REORDER_DELAY_MS" "REORDER_DELAY_MS" 1 600000
            validate_configuration
            require_root
            apply_impairment both reorder "$value %; auxiliary delay ${REORDER_DELAY_MS} ms" \
                delay "${REORDER_DELAY_MS}ms" reorder "${value}%"
            ;;
        outage)
            (( $# == 1 || $# == 2 )) || die "'outage' expects zero or one argument; use --help"
            validate_configuration
            require_root

            if (( $# == 1 )); then
                apply_impairment both outage "100 % packet loss until clear" loss 100%
                return
            fi

            value="$2"
            validate_integer_range "$value" "outage duration" 1 3600
            NETEM_HANDLE=$(timed_netem_handle)
            TIMED_OUTAGE=1
            apply_impairment both outage "100 % packet loss for $value s" loss 100%
            printf '\nOutage active for %s second(s); Ctrl-C also triggers cleanup.\n' "$value"
            sleep "$value"

            operation_status=0
            current_operation_is_active || operation_status=$?
            case "$operation_status" in
                0)
                    remove_owned_qdisc "$APPLIED_IFACE"
                    printf 'Timed outage finished; nominal qdisc restored.\n'
                    ;;
                1)
                    printf 'Timed outage finished; its qdisc was already cleared or replaced, so no newer impairment was removed.\n'
                    ;;
                2)
                    printf 'Timed outage finished, but qdisc state could not be verified; run clear manually.\n' >&2
                    ;;
            esac
            show_tc_state "$APPLIED_IFACE"
            TIMED_OUTAGE=0
            APPLIED_IFACE=""
            APPLIED_NETEM_HANDLE=""
            trap - EXIT INT TERM
            ;;
        channel1-loss)
            expect_argument_count "$#" 1 "$command"
            validate_configuration
            require_distinct_channels
            require_root
            apply_impairment channel1 channel1-loss "100 % packet loss on UDP destination $OC_CH1_PORT" loss 100%
            ;;
        channel2-loss)
            expect_argument_count "$#" 1 "$command"
            validate_configuration
            require_distinct_channels
            require_root
            apply_impairment channel2 channel2-loss "100 % packet loss on UDP destination $OC_CH2_PORT" loss 100%
            ;;
        *)
            die "unknown command '$command'; use --help"
            ;;
    esac
}

main "$@"
