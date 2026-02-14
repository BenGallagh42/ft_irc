#!/bin/bash

# ============================================================================
#                    ULTIMATE IRC SERVER TEST SUITE
#          Tests EVERY possible way the evaluation could fail
# ============================================================================

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
MAGENTA='\033[0;35m'
CYAN='\033[0;36m'
RESET='\033[0m'

TOTAL=0
PASSED=0
FAILED=0

PORT=6667
PASSWORD="test42"
SERVER_PID=""

print_test() {
    local name=$1
    local result=$2
    TOTAL=$((TOTAL + 1))
    if [ "$result" = "PASS" ]; then
        echo -e "${GREEN}✓${RESET} $name"
        PASSED=$((PASSED + 1))
    else
        echo -e "${RED}✗${RESET} $name"
        FAILED=$((FAILED + 1))
    fi
}

print_section() {
    echo ""
    echo -e "${CYAN}╔════════════════════════════════════════════════════════╗${RESET}"
    echo -e "${CYAN}║${RESET} $1"
    echo -e "${CYAN}╚════════════════════════════════════════════════════════╝${RESET}"
}

start_server() {
    echo -e "${BLUE}[INFO]${RESET} Starting server..."
    ./ircserv $PORT $PASSWORD > /tmp/ultimate_server.log 2>&1 &
    SERVER_PID=$!
    sleep 1
    if ! kill -0 $SERVER_PID 2>/dev/null; then
        echo -e "${RED}[FATAL]${RESET} Server failed to start"
        exit 1
    fi
    echo -e "${GREEN}[OK]${RESET} Server running (PID: $SERVER_PID)"
}

stop_server() {
    if [ ! -z "$SERVER_PID" ]; then
        kill $SERVER_PID 2>/dev/null
        wait $SERVER_PID 2>/dev/null
    fi
}

cleanup() {
    stop_server
    rm -f /tmp/ult_*.log
}
trap cleanup EXIT

echo -e "${MAGENTA}"
echo "╔══════════════════════════════════════════════════════════════╗"
echo "║                                                              ║"
echo "║           ULTIMATE IRC SERVER TEST SUITE                     ║"
echo "║                  ft_irc Evaluation                           ║"
echo "║                                                              ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo -e "${RESET}"

start_server

# ============================================================================
#                    PHASE 1: AUTHENTICATION
# ============================================================================

print_section "PHASE 1: Authentication Tests"

# Test 1.1: Valid authentication
{ printf "PASS $PASSWORD\r\nNICK test1\r\nUSER test1 0 * :Test1\r\n"; sleep 1; } | timeout 3 nc localhost $PORT > /tmp/ult_auth1.log 2>&1
[ -f /tmp/ult_auth1.log ] && grep -q "001" /tmp/ult_auth1.log && print_test "Valid PASS/NICK/USER" "PASS" || print_test "Valid PASS/NICK/USER" "FAIL"

# Test 1.2: Wrong password
{ printf "PASS wrongpass\r\nNICK test2\r\nUSER test2 0 * :Test2\r\n"; sleep 1; } | timeout 3 nc localhost $PORT > /tmp/ult_auth2.log 2>&1
[ -f /tmp/ult_auth2.log ] && grep -q "464" /tmp/ult_auth2.log && print_test "Wrong password rejected (464)" "PASS" || print_test "Wrong password rejected (464)" "FAIL"

# Test 1.3: No password
{ printf "NICK test3\r\nUSER test3 0 * :Test3\r\n"; sleep 1; } | timeout 3 nc localhost $PORT > /tmp/ult_auth3.log 2>&1
[ -f /tmp/ult_auth3.log ] && grep -q "464" /tmp/ult_auth3.log && print_test "No password rejected (464)" "PASS" || print_test "No password rejected (464)" "FAIL"

# Test 1.4: Nickname already in use
{ printf "PASS $PASSWORD\r\nNICK duplicate\r\nUSER dup1 0 * :Dup1\r\n"; sleep 10; } | timeout 15 nc localhost $PORT > /dev/null 2>&1 &
sleep 2
{ printf "PASS $PASSWORD\r\nNICK duplicate\r\nUSER dup2 0 * :Dup2\r\n"; sleep 1; } | timeout 3 nc localhost $PORT > /tmp/ult_auth4.log 2>&1
[ -f /tmp/ult_auth4.log ] && grep -q "433" /tmp/ult_auth4.log && print_test "Nickname collision (433)" "PASS" || print_test "Nickname collision (433)" "FAIL"

# Test 1.5: Empty nickname
{ printf "PASS $PASSWORD\r\nNICK\r\n"; sleep 1; } | timeout 3 nc localhost $PORT > /tmp/ult_auth5.log 2>&1
[ -f /tmp/ult_auth5.log ] && grep -q "431" /tmp/ult_auth5.log && print_test "Empty nickname rejected (431)" "PASS" || print_test "Empty nickname rejected (431)" "FAIL"

# Test 1.6: Invalid nickname characters
{ printf "PASS $PASSWORD\r\nNICK bad@nick\r\n"; sleep 1; } | timeout 3 nc localhost $PORT > /tmp/ult_auth6.log 2>&1
[ -f /tmp/ult_auth6.log ] && grep -q "432" /tmp/ult_auth6.log && print_test "Invalid nickname rejected (432)" "PASS" || print_test "Invalid nickname rejected (432)" "FAIL"

# Test 1.7: Commands before registration
{ printf "PASS $PASSWORD\r\nJOIN #test\r\n"; sleep 1; } | timeout 3 nc localhost $PORT > /tmp/ult_auth7.log 2>&1
[ -f /tmp/ult_auth7.log ] && grep -q "451" /tmp/ult_auth7.log && print_test "Commands before registration blocked (451)" "PASS" || print_test "Commands before registration blocked (451)" "FAIL"

# ============================================================================
#                    PHASE 2: CHANNEL OPERATIONS
# ============================================================================

print_section "PHASE 2: Channel Operations"

# Test 2.1: JOIN creates channel
{ printf "PASS $PASSWORD\r\nNICK chan1\r\nUSER chan1 0 * :Chan1\r\nJOIN #newchan\r\n"; sleep 1; } | timeout 3 nc localhost $PORT > /tmp/ult_chan1.log 2>&1
[ -f /tmp/ult_chan1.log ] && grep -q "JOIN" /tmp/ult_chan1.log && print_test "JOIN creates channel" "PASS" || print_test "JOIN creates channel" "FAIL"

# Test 2.2: First member becomes operator
{ printf "PASS $PASSWORD\r\nNICK chan2\r\nUSER chan2 0 * :Chan2\r\nJOIN #opchan\r\n"; sleep 1; } | timeout 3 nc localhost $PORT > /tmp/ult_chan2.log 2>&1
[ -f /tmp/ult_chan2.log ] && grep -q "@chan2" /tmp/ult_chan2.log && print_test "First member becomes operator" "PASS" || print_test "First member becomes operator" "FAIL"

# Test 2.3: PART leaves channel
{ printf "PASS $PASSWORD\r\nNICK chan3\r\nUSER chan3 0 * :Chan3\r\nJOIN #parttest\r\nPART #parttest\r\n"; sleep 1; } | timeout 3 nc localhost $PORT > /tmp/ult_chan3.log 2>&1
[ -f /tmp/ult_chan3.log ] && grep -q "PART" /tmp/ult_chan3.log && print_test "PART leaves channel" "PASS" || print_test "PART leaves channel" "FAIL"

# Test 2.4: Empty channel deleted
{ printf "PASS $PASSWORD\r\nNICK chan4\r\nUSER chan4 0 * :Chan4\r\nJOIN #deltest\r\nPART #deltest\r\nJOIN #deltest\r\n"; sleep 1; } | timeout 3 nc localhost $PORT > /tmp/ult_chan4.log 2>&1
[ -f /tmp/ult_chan4.log ] && grep -c "JOIN" /tmp/ult_chan4.log | grep -q "2" && print_test "Empty channel deleted and recreated" "PASS" || print_test "Empty channel deleted and recreated" "FAIL"

# Test 2.5: JOIN without parameter
{ printf "PASS $PASSWORD\r\nNICK chan5\r\nUSER chan5 0 * :Chan5\r\nJOIN\r\n"; sleep 1; } | timeout 3 nc localhost $PORT > /tmp/ult_chan5.log 2>&1
[ -f /tmp/ult_chan5.log ] && grep -q "461" /tmp/ult_chan5.log && print_test "JOIN without parameter (461)" "PASS" || print_test "JOIN without parameter (461)" "FAIL"

# Test 2.6: Invalid channel name
{ printf "PASS $PASSWORD\r\nNICK chan6\r\nUSER chan6 0 * :Chan6\r\nJOIN invalid\r\n"; sleep 1; } | timeout 3 nc localhost $PORT > /tmp/ult_chan6.log 2>&1
[ -f /tmp/ult_chan6.log ] && grep -q "476\|403" /tmp/ult_chan6.log && print_test "Invalid channel name rejected (476)" "PASS" || print_test "Invalid channel name rejected (476)" "FAIL"

# Test 2.7: PART without being in channel
{ printf "PASS $PASSWORD\r\nNICK chan7\r\nUSER chan7 0 * :Chan7\r\nPART #nowhere\r\n"; sleep 1; } | timeout 3 nc localhost $PORT > /tmp/ult_chan7.log 2>&1
[ -f /tmp/ult_chan7.log ] && grep -q "442\|403" /tmp/ult_chan7.log && print_test "PART non-joined channel (442)" "PASS" || print_test "PART non-joined channel (442)" "FAIL"

# ============================================================================
#                    PHASE 3: MESSAGES
# ============================================================================

print_section "PHASE 3: Message Tests"

# Test 3.1: Channel message
{ printf "PASS $PASSWORD\r\nNICK msg1a\r\nUSER msg1a 0 * :Msg1a\r\nJOIN #msgtest\r\n"; sleep 10; } | timeout 15 nc localhost $PORT > /tmp/ult_msg1a.log 2>&1 &
sleep 2
{ printf "PASS $PASSWORD\r\nNICK msg1b\r\nUSER msg1b 0 * :Msg1b\r\nJOIN #msgtest\r\nPRIVMSG #msgtest :Hello\r\n"; sleep 1; } | timeout 3 nc localhost $PORT > /tmp/ult_msg1b.log 2>&1
sleep 1
[ -f /tmp/ult_msg1a.log ] && grep -q "Hello" /tmp/ult_msg1a.log && print_test "Channel message broadcast" "PASS" || print_test "Channel message broadcast" "FAIL"

# Test 3.2: Private message
{ printf "PASS $PASSWORD\r\nNICK msg2a\r\nUSER msg2a 0 * :Msg2a\r\n"; sleep 10; } | timeout 15 nc localhost $PORT > /tmp/ult_msg2a.log 2>&1 &
sleep 2
{ printf "PASS $PASSWORD\r\nNICK msg2b\r\nUSER msg2b 0 * :Msg2b\r\nPRIVMSG msg2a :Private\r\n"; sleep 1; } | timeout 3 nc localhost $PORT > /tmp/ult_msg2b.log 2>&1
sleep 1
[ -f /tmp/ult_msg2a.log ] && grep -q "Private" /tmp/ult_msg2a.log && print_test "Private message delivery" "PASS" || print_test "Private message delivery" "FAIL"

# Test 3.3: PRIVMSG without recipient
{ printf "PASS $PASSWORD\r\nNICK msg3\r\nUSER msg3 0 * :Msg3\r\nPRIVMSG\r\n"; sleep 1; } | timeout 3 nc localhost $PORT > /tmp/ult_msg3.log 2>&1
[ -f /tmp/ult_msg3.log ] && grep -q "411" /tmp/ult_msg3.log && print_test "PRIVMSG without recipient (411)" "PASS" || print_test "PRIVMSG without recipient (411)" "FAIL"

# Test 3.4: PRIVMSG without text
{ printf "PASS $PASSWORD\r\nNICK msg4\r\nUSER msg4 0 * :Msg4\r\nPRIVMSG #test\r\n"; sleep 1; } | timeout 3 nc localhost $PORT > /tmp/ult_msg4.log 2>&1
[ -f /tmp/ult_msg4.log ] && grep -q "412" /tmp/ult_msg4.log && print_test "PRIVMSG without text (412)" "PASS" || print_test "PRIVMSG without text (412)" "FAIL"

# Test 3.5: PRIVMSG to non-existent user
{ printf "PASS $PASSWORD\r\nNICK msg5\r\nUSER msg5 0 * :Msg5\r\nPRIVMSG nobody :Hi\r\n"; sleep 1; } | timeout 3 nc localhost $PORT > /tmp/ult_msg5.log 2>&1
[ -f /tmp/ult_msg5.log ] && grep -q "401" /tmp/ult_msg5.log && print_test "PRIVMSG to non-existent user (401)" "PASS" || print_test "PRIVMSG to non-existent user (401)" "FAIL"

# Test 3.6: PRIVMSG to channel not joined
{ printf "PASS $PASSWORD\r\nNICK msg6a\r\nUSER msg6a 0 * :Msg6a\r\nJOIN #private\r\n"; sleep 10; } | timeout 15 nc localhost $PORT > /dev/null 2>&1 &
sleep 2
{ printf "PASS $PASSWORD\r\nNICK msg6b\r\nUSER msg6b 0 * :Msg6b\r\nPRIVMSG #private :Test\r\n"; sleep 1; } | timeout 3 nc localhost $PORT > /tmp/ult_msg6.log 2>&1
[ -f /tmp/ult_msg6.log ] && grep -q "442\|403" /tmp/ult_msg6.log && print_test "PRIVMSG to non-joined channel (442)" "PASS" || print_test "PRIVMSG to non-joined channel (442)" "FAIL"

# ============================================================================
#                    PHASE 4: KICK COMMAND
# ============================================================================

print_section "PHASE 4: KICK Command Tests"

# Test 4.1: Operator can KICK
{ printf "PASS $PASSWORD\r\nNICK kick1a\r\nUSER kick1a 0 * :Kick1a\r\nJOIN #kicktest\r\n"; sleep 10; } | timeout 15 nc localhost $PORT > /dev/null 2>&1 &
sleep 2
{ printf "PASS $PASSWORD\r\nNICK kick1b\r\nUSER kick1b 0 * :Kick1b\r\nJOIN #kicktest\r\n"; sleep 10; } | timeout 15 nc localhost $PORT > /tmp/ult_kick1b.log 2>&1 &
sleep 2
{ printf "KICK #kicktest kick1b :Bye\r\n"; sleep 1; } | timeout 3 nc localhost $PORT > /dev/null 2>&1
sleep 1
[ -f /tmp/ult_kick1b.log ] && grep -q "KICK" /tmp/ult_kick1b.log && print_test "Operator can KICK" "PASS" || print_test "Operator can KICK" "FAIL"

# Test 4.2: Non-operator cannot KICK
{ printf "PASS $PASSWORD\r\nNICK kick2a\r\nUSER kick2a 0 * :Kick2a\r\nJOIN #kicktest2\r\n"; sleep 10; } | timeout 15 nc localhost $PORT > /dev/null 2>&1 &
sleep 2
{ printf "PASS $PASSWORD\r\nNICK kick2b\r\nUSER kick2b 0 * :Kick2b\r\nJOIN #kicktest2\r\nKICK #kicktest2 kick2a\r\n"; sleep 1; } | timeout 3 nc localhost $PORT > /tmp/ult_kick2.log 2>&1
[ -f /tmp/ult_kick2.log ] && grep -q "482" /tmp/ult_kick2.log && print_test "Non-op cannot KICK (482)" "PASS" || print_test "Non-op cannot KICK (482)" "FAIL"

# Test 4.3: KICK without parameters
{ printf "PASS $PASSWORD\r\nNICK kick3\r\nUSER kick3 0 * :Kick3\r\nJOIN #kicktest3\r\nKICK\r\n"; sleep 1; } | timeout 3 nc localhost $PORT > /tmp/ult_kick3.log 2>&1
[ -f /tmp/ult_kick3.log ] && grep -q "461" /tmp/ult_kick3.log && print_test "KICK without parameters (461)" "PASS" || print_test "KICK without parameters (461)" "FAIL"

# Test 4.4: KICK non-existent user
{ printf "PASS $PASSWORD\r\nNICK kick4\r\nUSER kick4 0 * :Kick4\r\nJOIN #kicktest4\r\nKICK #kicktest4 nobody\r\n"; sleep 1; } | timeout 3 nc localhost $PORT > /tmp/ult_kick4.log 2>&1
[ -f /tmp/ult_kick4.log ] && grep -q "401" /tmp/ult_kick4.log && print_test "KICK non-existent user (401)" "PASS" || print_test "KICK non-existent user (401)" "FAIL"

# Test 4.5: KICK user not in channel
{ printf "PASS $PASSWORD\r\nNICK kick5a\r\nUSER kick5a 0 * :Kick5a\r\nJOIN #kicktest5\r\n"; sleep 10; } | timeout 15 nc localhost $PORT > /dev/null 2>&1 &
sleep 2
{ printf "PASS $PASSWORD\r\nNICK kick5b\r\nUSER kick5b 0 * :Kick5b\r\n"; sleep 1; } | timeout 3 nc localhost $PORT > /dev/null 2>&1 &
sleep 1
{ printf "KICK #kicktest5 kick5b\r\n"; sleep 1; } | timeout 3 nc localhost $PORT > /tmp/ult_kick5.log 2>&1
[ -f /tmp/ult_kick5.log ] && grep -q "441" /tmp/ult_kick5.log && print_test "KICK user not in channel (441)" "PASS" || print_test "KICK user not in channel (441)" "FAIL"

# ============================================================================
#                    PHASE 5: INVITE COMMAND
# ============================================================================

print_section "PHASE 5: INVITE Command Tests"

# Test 5.1: Operator can INVITE
{ printf "PASS $PASSWORD\r\nNICK inv1a\r\nUSER inv1a 0 * :Inv1a\r\nJOIN #invtest\r\nINVITE inv1b #invtest\r\n"; sleep 10; } | timeout 15 nc localhost $PORT > /dev/null 2>&1 &
sleep 2
{ printf "PASS $PASSWORD\r\nNICK inv1b\r\nUSER inv1b 0 * :Inv1b\r\n"; sleep 10; } | timeout 15 nc localhost $PORT > /tmp/ult_inv1b.log 2>&1 &
sleep 2
[ -f /tmp/ult_inv1b.log ] && grep -q "INVITE" /tmp/ult_inv1b.log && print_test "Operator can INVITE" "PASS" || print_test "Operator can INVITE" "FAIL"

# Test 5.2: Non-operator cannot INVITE
{ printf "PASS $PASSWORD\r\nNICK inv2a\r\nUSER inv2a 0 * :Inv2a\r\nJOIN #invtest2\r\n"; sleep 10; } | timeout 15 nc localhost $PORT > /dev/null 2>&1 &
sleep 2
{ printf "PASS $PASSWORD\r\nNICK inv2b\r\nUSER inv2b 0 * :Inv2b\r\nJOIN #invtest2\r\nINVITE inv2c #invtest2\r\n"; sleep 1; } | timeout 3 nc localhost $PORT > /tmp/ult_inv2.log 2>&1
[ -f /tmp/ult_inv2.log ] && grep -q "482" /tmp/ult_inv2.log && print_test "Non-op cannot INVITE (482)" "PASS" || print_test "Non-op cannot INVITE (482)" "FAIL"

# Test 5.3: INVITE without parameters
{ printf "PASS $PASSWORD\r\nNICK inv3\r\nUSER inv3 0 * :Inv3\r\nJOIN #invtest3\r\nINVITE\r\n"; sleep 1; } | timeout 3 nc localhost $PORT > /tmp/ult_inv3.log 2>&1
[ -f /tmp/ult_inv3.log ] && grep -q "461" /tmp/ult_inv3.log && print_test "INVITE without parameters (461)" "PASS" || print_test "INVITE without parameters (461)" "FAIL"

# Test 5.4: INVITE non-existent user
{ printf "PASS $PASSWORD\r\nNICK inv4\r\nUSER inv4 0 * :Inv4\r\nJOIN #invtest4\r\nINVITE nobody #invtest4\r\n"; sleep 1; } | timeout 3 nc localhost $PORT > /tmp/ult_inv4.log 2>&1
[ -f /tmp/ult_inv4.log ] && grep -q "401" /tmp/ult_inv4.log && print_test "INVITE non-existent user (401)" "PASS" || print_test "INVITE non-existent user (401)" "FAIL"

# Test 5.5: INVITE to non-existent channel
{ printf "PASS $PASSWORD\r\nNICK inv5a\r\nUSER inv5a 0 * :Inv5a\r\n"; sleep 10; } | timeout 15 nc localhost $PORT > /dev/null 2>&1 &
sleep 2
{ printf "PASS $PASSWORD\r\nNICK inv5b\r\nUSER inv5b 0 * :Inv5b\r\nINVITE inv5a #nowhere\r\n"; sleep 1; } | timeout 3 nc localhost $PORT > /tmp/ult_inv5.log 2>&1
[ -f /tmp/ult_inv5.log ] && grep -q "403\|442" /tmp/ult_inv5.log && print_test "INVITE to non-existent channel (403)" "PASS" || print_test "INVITE to non-existent channel (403)" "FAIL"

# Test 5.6: INVITE user already in channel
{ printf "PASS $PASSWORD\r\nNICK inv6a\r\nUSER inv6a 0 * :Inv6a\r\nJOIN #invtest6\r\n"; sleep 10; } | timeout 15 nc localhost $PORT > /dev/null 2>&1 &
sleep 2
{ printf "PASS $PASSWORD\r\nNICK inv6b\r\nUSER inv6b 0 * :Inv6b\r\nJOIN #invtest6\r\n"; sleep 1; } | timeout 3 nc localhost $PORT > /dev/null 2>&1 &
sleep 1
{ printf "INVITE inv6b #invtest6\r\n"; sleep 1; } | timeout 3 nc localhost $PORT > /tmp/ult_inv6.log 2>&1
[ -f /tmp/ult_inv6.log ] && grep -q "443" /tmp/ult_inv6.log && print_test "INVITE user already in channel (443)" "PASS" || print_test "INVITE user already in channel (443)" "FAIL"

# ============================================================================
#                    PHASE 6: TOPIC COMMAND
# ============================================================================

print_section "PHASE 6: TOPIC Command Tests"

# Test 6.1: View topic
{ printf "PASS $PASSWORD\r\nNICK top1\r\nUSER top1 0 * :Top1\r\nJOIN #toptest\r\nTOPIC #toptest\r\n"; sleep 1; } | timeout 3 nc localhost $PORT > /tmp/ult_top1.log 2>&1
[ -f /tmp/ult_top1.log ] && grep -q "331\|332" /tmp/ult_top1.log && print_test "View topic (331/332)" "PASS" || print_test "View topic (331/332)" "FAIL"

# Test 6.2: Set topic (no +t)
{ printf "PASS $PASSWORD\r\nNICK top2a\r\nUSER top2a 0 * :Top2a\r\nJOIN #toptest2\r\n"; sleep 10; } | timeout 15 nc localhost $PORT > /dev/null 2>&1 &
sleep 2
{ printf "PASS $PASSWORD\r\nNICK top2b\r\nUSER top2b 0 * :Top2b\r\nJOIN #toptest2\r\nTOPIC #toptest2 :New\r\n"; sleep 1; } | timeout 3 nc localhost $PORT > /tmp/ult_top2.log 2>&1
[ -f /tmp/ult_top2.log ] && grep -q "TOPIC.*New" /tmp/ult_top2.log && print_test "Non-op can set topic (no +t)" "PASS" || print_test "Non-op can set topic (no +t)" "FAIL"

# Test 6.3: Operator sets topic with +t
{ printf "PASS $PASSWORD\r\nNICK top3\r\nUSER top3 0 * :Top3\r\nJOIN #toptest3\r\nMODE #toptest3 +t\r\nTOPIC #toptest3 :OpTopic\r\n"; sleep 1; } | timeout 3 nc localhost $PORT > /tmp/ult_top3.log 2>&1
[ -f /tmp/ult_top3.log ] && grep -q "TOPIC.*OpTopic" /tmp/ult_top3.log && print_test "Op can set topic with +t" "PASS" || print_test "Op can set topic with +t" "FAIL"

# Test 6.4: Non-op cannot set topic with +t
{ printf "PASS $PASSWORD\r\nNICK top4a\r\nUSER top4a 0 * :Top4a\r\nJOIN #toptest4\r\nMODE #toptest4 +t\r\n"; sleep 10; } | timeout 15 nc localhost $PORT > /dev/null 2>&1 &
sleep 2
{ printf "PASS $PASSWORD\r\nNICK top4b\r\nUSER top4b 0 * :Top4b\r\nJOIN #toptest4\r\nTOPIC #toptest4 :Fail\r\n"; sleep 1; } | timeout 3 nc localhost $PORT > /tmp/ult_top4.log 2>&1
[ -f /tmp/ult_top4.log ] && grep -q "482" /tmp/ult_top4.log && print_test "Non-op cannot set topic with +t (482)" "PASS" || print_test "Non-op cannot set topic with +t (482)" "FAIL"

# Test 6.5: TOPIC without parameter shows topic
{ printf "PASS $PASSWORD\r\nNICK top5\r\nUSER top5 0 * :Top5\r\nJOIN #toptest5\r\nTOPIC #toptest5 :TestTopic\r\nTOPIC #toptest5\r\n"; sleep 1; } | timeout 3 nc localhost $PORT > /tmp/ult_top5.log 2>&1
[ -f /tmp/ult_top5.log ] && grep -q "TestTopic" /tmp/ult_top5.log && print_test "TOPIC without param shows topic" "PASS" || print_test "TOPIC without param shows topic" "FAIL"

# ============================================================================
#                    PHASE 7: MODE +i (Invite Only)
# ============================================================================

print_section "PHASE 7: MODE +i Tests"

# Test 7.1: Set +i mode
{ printf "PASS $PASSWORD\r\nNICK modei1\r\nUSER modei1 0 * :ModeI1\r\nJOIN #itest\r\nMODE #itest +i\r\nMODE #itest\r\n"; sleep 1; } | timeout 3 nc localhost $PORT > /tmp/ult_modei1.log 2>&1
[ -f /tmp/ult_modei1.log ] && grep -q "324.*+.*i" /tmp/ult_modei1.log && print_test "Set MODE +i" "PASS" || print_test "Set MODE +i" "FAIL"

# Test 7.2: Cannot join +i without invite
{ printf "PASS $PASSWORD\r\nNICK modei2a\r\nUSER modei2a 0 * :ModeI2a\r\nJOIN #itest2\r\nMODE #itest2 +i\r\n"; sleep 10; } | timeout 15 nc localhost $PORT > /dev/null 2>&1 &
sleep 2
{ printf "PASS $PASSWORD\r\nNICK modei2b\r\nUSER modei2b 0 * :ModeI2b\r\nJOIN #itest2\r\n"; sleep 1; } | timeout 3 nc localhost $PORT > /tmp/ult_modei2.log 2>&1
[ -f /tmp/ult_modei2.log ] && grep -q "473" /tmp/ult_modei2.log && print_test "Cannot join +i without invite (473)" "PASS" || print_test "Cannot join +i without invite (473)" "FAIL"

# Test 7.3: Can join +i with invite
{ printf "PASS $PASSWORD\r\nNICK modei3a\r\nUSER modei3a 0 * :ModeI3a\r\nJOIN #itest3\r\nMODE #itest3 +i\r\nINVITE modei3b #itest3\r\n"; sleep 10; } | timeout 15 nc localhost $PORT > /dev/null 2>&1 &
sleep 2
{ printf "PASS $PASSWORD\r\nNICK modei3b\r\nUSER modei3b 0 * :ModeI3b\r\nJOIN #itest3\r\n"; sleep 1; } | timeout 3 nc localhost $PORT > /tmp/ult_modei3.log 2>&1
[ -f /tmp/ult_modei3.log ] && grep -q "JOIN" /tmp/ult_modei3.log && print_test "Can join +i with invite" "PASS" || print_test "Can join +i with invite" "FAIL"

# Test 7.4: Remove -i mode
{ printf "PASS $PASSWORD\r\nNICK modei4\r\nUSER modei4 0 * :ModeI4\r\nJOIN #itest4\r\nMODE #itest4 +i\r\nMODE #itest4 -i\r\nMODE #itest4\r\n"; sleep 1; } | timeout 3 nc localhost $PORT > /tmp/ult_modei4.log 2>&1
[ -f /tmp/ult_modei4.log ] && ! grep -q "324.*+.*i" /tmp/ult_modei4.log && print_test "Remove MODE -i" "PASS" || print_test "Remove MODE -i" "FAIL"

# Test 7.5: Non-op cannot set +i
{ printf "PASS $PASSWORD\r\nNICK modei5a\r\nUSER modei5a 0 * :ModeI5a\r\nJOIN #itest5\r\n"; sleep 10; } | timeout 15 nc localhost $PORT > /dev/null 2>&1 &
sleep 2
{ printf "PASS $PASSWORD\r\nNICK modei5b\r\nUSER modei5b 0 * :ModeI5b\r\nJOIN #itest5\r\nMODE #itest5 +i\r\n"; sleep 1; } | timeout 3 nc localhost $PORT > /tmp/ult_modei5.log 2>&1
[ -f /tmp/ult_modei5.log ] && grep -q "482" /tmp/ult_modei5.log && print_test "Non-op cannot set +i (482)" "PASS" || print_test "Non-op cannot set +i (482)" "FAIL"

# ============================================================================
#                    PHASE 8: MODE +t (Topic Restricted)
# ============================================================================

print_section "PHASE 8: MODE +t Tests"

# Test 8.1: Set +t mode
{ printf "PASS $PASSWORD\r\nNICK modet1\r\nUSER modet1 0 * :ModeT1\r\nJOIN #ttest\r\nMODE #ttest +t\r\nMODE #ttest\r\n"; sleep 1; } | timeout 3 nc localhost $PORT > /tmp/ult_modet1.log 2>&1
[ -f /tmp/ult_modet1.log ] && grep -q "324.*+.*t" /tmp/ult_modet1.log && print_test "Set MODE +t" "PASS" || print_test "Set MODE +t" "FAIL"

# Test 8.2: Remove -t mode
{ printf "PASS $PASSWORD\r\nNICK modet2\r\nUSER modet2 0 * :ModeT2\r\nJOIN #ttest2\r\nMODE #ttest2 +t\r\nMODE #ttest2 -t\r\nMODE #ttest2\r\n"; sleep 1; } | timeout 3 nc localhost $PORT > /tmp/ult_modet2.log 2>&1
[ -f /tmp/ult_modet2.log ] && ! grep -q "324.*+.*t" /tmp/ult_modet2.log && print_test "Remove MODE -t" "PASS" || print_test "Remove MODE -t" "FAIL"

# Test 8.3: Non-op cannot set +t
{ printf "PASS $PASSWORD\r\nNICK modet3a\r\nUSER modet3a 0 * :ModeT3a\r\nJOIN #ttest3\r\n"; sleep 10; } | timeout 15 nc localhost $PORT > /dev/null 2>&1 &
sleep 2
{ printf "PASS $PASSWORD\r\nNICK modet3b\r\nUSER modet3b 0 * :ModeT3b\r\nJOIN #ttest3\r\nMODE #ttest3 +t\r\n"; sleep 1; } | timeout 3 nc localhost $PORT > /tmp/ult_modet3.log 2>&1
[ -f /tmp/ult_modet3.log ] && grep -q "482" /tmp/ult_modet3.log && print_test "Non-op cannot set +t (482)" "PASS" || print_test "Non-op cannot set +t (482)" "FAIL"

# ============================================================================
#                    PHASE 9: MODE +k (Password)
# ============================================================================

print_section "PHASE 9: MODE +k Tests"

# Test 9.1: Set +k mode
{ printf "PASS $PASSWORD\r\nNICK modek1\r\nUSER modek1 0 * :ModeK1\r\nJOIN #ktest\r\nMODE #ktest +k secret\r\nMODE #ktest\r\n"; sleep 1; } | timeout 3 nc localhost $PORT > /tmp/ult_modek1.log 2>&1
[ -f /tmp/ult_modek1.log ] && grep -q "324.*+.*k" /tmp/ult_modek1.log && print_test "Set MODE +k" "PASS" || print_test "Set MODE +k" "FAIL"

# Test 9.2: Cannot join +k without password
{ printf "PASS $PASSWORD\r\nNICK modek2a\r\nUSER modek2a 0 * :ModeK2a\r\nJOIN #ktest2\r\nMODE #ktest2 +k pass123\r\n"; sleep 10; } | timeout 15 nc localhost $PORT > /dev/null 2>&1 &
sleep 2
{ printf "PASS $PASSWORD\r\nNICK modek2b\r\nUSER modek2b 0 * :ModeK2b\r\nJOIN #ktest2\r\n"; sleep 1; } | timeout 3 nc localhost $PORT > /tmp/ult_modek2.log 2>&1
[ -f /tmp/ult_modek2.log ] && grep -q "475" /tmp/ult_modek2.log && print_test "Cannot join +k without password (475)" "PASS" || print_test "Cannot join +k without password (475)" "FAIL"

# Test 9.3: Cannot join +k with wrong password
{ printf "PASS $PASSWORD\r\nNICK modek3a\r\nUSER modek3a 0 * :ModeK3a\r\nJOIN #ktest3\r\nMODE #ktest3 +k correct\r\n"; sleep 10; } | timeout 15 nc localhost $PORT > /dev/null 2>&1 &
sleep 2
{ printf "PASS $PASSWORD\r\nNICK modek3b\r\nUSER modek3b 0 * :ModeK3b\r\nJOIN #ktest3 wrong\r\n"; sleep 1; } | timeout 3 nc localhost $PORT > /tmp/ult_modek3.log 2>&1
[ -f /tmp/ult_modek3.log ] && grep -q "475" /tmp/ult_modek3.log && print_test "Cannot join +k with wrong password (475)" "PASS" || print_test "Cannot join +k with wrong password (475)" "FAIL"

# Test 9.4: Can join +k with correct password
{ printf "PASS $PASSWORD\r\nNICK modek4a\r\nUSER modek4a 0 * :ModeK4a\r\nJOIN #ktest4\r\nMODE #ktest4 +k goodpass\r\n"; sleep 10; } | timeout 15 nc localhost $PORT > /dev/null 2>&1 &
sleep 2
{ printf "PASS $PASSWORD\r\nNICK modek4b\r\nUSER modek4b 0 * :ModeK4b\r\nJOIN #ktest4 goodpass\r\n"; sleep 1; } | timeout 3 nc localhost $PORT > /tmp/ult_modek4.log 2>&1
[ -f /tmp/ult_modek4.log ] && grep -q "JOIN" /tmp/ult_modek4.log && print_test "Can join +k with correct password" "PASS" || print_test "Can join +k with correct password" "FAIL"

# Test 9.5: Remove -k mode
{ printf "PASS $PASSWORD\r\nNICK modek5\r\nUSER modek5 0 * :ModeK5\r\nJOIN #ktest5\r\nMODE #ktest5 +k pass\r\nMODE #ktest5 -k\r\nMODE #ktest5\r\n"; sleep 1; } | timeout 3 nc localhost $PORT > /tmp/ult_modek5.log 2>&1
[ -f /tmp/ult_modek5.log ] && ! grep -q "324.*+.*k" /tmp/ult_modek5.log && print_test "Remove MODE -k" "PASS" || print_test "Remove MODE -k" "FAIL"

# Test 9.6: MODE +k without password
{ printf "PASS $PASSWORD\r\nNICK modek6\r\nUSER modek6 0 * :ModeK6\r\nJOIN #ktest6\r\nMODE #ktest6 +k\r\n"; sleep 1; } | timeout 3 nc localhost $PORT > /tmp/ult_modek6.log 2>&1
[ -f /tmp/ult_modek6.log ] && grep -q "461" /tmp/ult_modek6.log && print_test "MODE +k without password (461)" "PASS" || print_test "MODE +k without password (461)" "FAIL"

# Test 9.7: Non-op cannot set +k
{ printf "PASS $PASSWORD\r\nNICK modek7a\r\nUSER modek7a 0 * :ModeK7a\r\nJOIN #ktest7\r\n"; sleep 10; } | timeout 15 nc localhost $PORT > /dev/null 2>&1 &
sleep 2
{ printf "PASS $PASSWORD\r\nNICK modek7b\r\nUSER modek7b 0 * :ModeK7b\r\nJOIN #ktest7\r\nMODE #ktest7 +k pass\r\n"; sleep 1; } | timeout 3 nc localhost $PORT > /tmp/ult_modek7.log 2>&1
[ -f /tmp/ult_modek7.log ] && grep -q "482" /tmp/ult_modek7.log && print_test "Non-op cannot set +k (482)" "PASS" || print_test "Non-op cannot set +k (482)" "FAIL"

# ============================================================================
#                    PHASE 10: MODE +o (Operator)
# ============================================================================

print_section "PHASE 10: MODE +o Tests"

# Test 10.1: Give +o to user
{ printf "PASS $PASSWORD\r\nNICK modeo1a\r\nUSER modeo1a 0 * :ModeO1a\r\nJOIN #otest\r\n"; sleep 10; } | timeout 15 nc localhost $PORT > /dev/null 2>&1 &
sleep 2
{ printf "PASS $PASSWORD\r\nNICK modeo1b\r\nUSER modeo1b 0 * :ModeO1b\r\nJOIN #otest\r\n"; sleep 10; } | timeout 15 nc localhost $PORT > /tmp/ult_modeo1b.log 2>&1 &
sleep 2
{ printf "MODE #otest +o modeo1b\r\n"; sleep 1; } | timeout 3 nc localhost $PORT > /dev/null 2>&1
sleep 1
[ -f /tmp/ult_modeo1b.log ] && grep -q "MODE.*+o" /tmp/ult_modeo1b.log && print_test "Give MODE +o to user" "PASS" || print_test "Give MODE +o to user" "FAIL"

# Test 10.2: New operator can use operator commands
{ printf "PASS $PASSWORD\r\nNICK modeo2a\r\nUSER modeo2a 0 * :ModeO2a\r\nJOIN #otest2\r\n"; sleep 10; } | timeout 15 nc localhost $PORT > /dev/null 2>&1 &
sleep 2
{ printf "PASS $PASSWORD\r\nNICK modeo2b\r\nUSER modeo2b 0 * :ModeO2b\r\nJOIN #otest2\r\n"; sleep 10; } | timeout 15 nc localhost $PORT > /dev/null 2>&1 &
sleep 2
{ printf "MODE #otest2 +o modeo2b\r\n"; sleep 1; } | timeout 3 nc localhost $PORT > /dev/null 2>&1
sleep 1
{ printf "MODE #otest2 +i\r\n"; sleep 1; } | timeout 3 nc localhost $PORT > /tmp/ult_modeo2.log 2>&1
[ -f /tmp/ult_modeo2.log ] && grep -q "MODE.*+i" /tmp/ult_modeo2.log && print_test "New operator can use MODE" "PASS" || print_test "New operator can use MODE" "FAIL"

# Test 10.3: Remove -o from user
{ printf "PASS $PASSWORD\r\nNICK modeo3a\r\nUSER modeo3a 0 * :ModeO3a\r\nJOIN #otest3\r\n"; sleep 10; } | timeout 15 nc localhost $PORT > /dev/null 2>&1 &
sleep 2
{ printf "PASS $PASSWORD\r\nNICK modeo3b\r\nUSER modeo3b 0 * :ModeO3b\r\nJOIN #otest3\r\n"; sleep 10; } | timeout 15 nc localhost $PORT > /tmp/ult_modeo3b.log 2>&1 &
sleep 2
{ printf "MODE #otest3 +o modeo3b\r\n"; sleep 1; } | timeout 3 nc localhost $PORT > /dev/null 2>&1
sleep 1
{ printf "MODE #otest3 -o modeo3b\r\n"; sleep 1; } | timeout 3 nc localhost $PORT > /dev/null 2>&1
sleep 1
[ -f /tmp/ult_modeo3b.log ] && grep -q "MODE.*-o" /tmp/ult_modeo3b.log && print_test "Remove MODE -o from user" "PASS" || print_test "Remove MODE -o from user" "FAIL"

# Test 10.4: MODE +o without nickname
{ printf "PASS $PASSWORD\r\nNICK modeo4\r\nUSER modeo4 0 * :ModeO4\r\nJOIN #otest4\r\nMODE #otest4 +o\r\n"; sleep 1; } | timeout 3 nc localhost $PORT > /tmp/ult_modeo4.log 2>&1
[ -f /tmp/ult_modeo4.log ] && grep -q "461" /tmp/ult_modeo4.log && print_test "MODE +o without nickname (461)" "PASS" || print_test "MODE +o without nickname (461)" "FAIL"

# Test 10.5: MODE +o non-existent user
{ printf "PASS $PASSWORD\r\nNICK modeo5\r\nUSER modeo5 0 * :ModeO5\r\nJOIN #otest5\r\nMODE #otest5 +o nobody\r\n"; sleep 1; } | timeout 3 nc localhost $PORT > /tmp/ult_modeo5.log 2>&1
[ -f /tmp/ult_modeo5.log ] && grep -q "401" /tmp/ult_modeo5.log && print_test "MODE +o non-existent user (401)" "PASS" || print_test "MODE +o non-existent user (401)" "FAIL"

# Test 10.6: MODE +o user not in channel
{ printf "PASS $PASSWORD\r\nNICK modeo6a\r\nUSER modeo6a 0 * :ModeO6a\r\nJOIN #otest6\r\n"; sleep 10; } | timeout 15 nc localhost $PORT > /dev/null 2>&1 &
sleep 2
{ printf "PASS $PASSWORD\r\nNICK modeo6b\r\nUSER modeo6b 0 * :ModeO6b\r\n"; sleep 1; } | timeout 3 nc localhost $PORT > /dev/null 2>&1 &
sleep 1
{ printf "MODE #otest6 +o modeo6b\r\n"; sleep 1; } | timeout 3 nc localhost $PORT > /tmp/ult_modeo6.log 2>&1
[ -f /tmp/ult_modeo6.log ] && grep -q "441" /tmp/ult_modeo6.log && print_test "MODE +o user not in channel (441)" "PASS" || print_test "MODE +o user not in channel (441)" "FAIL"

# Test 10.7: Non-op cannot give +o
{ printf "PASS $PASSWORD\r\nNICK modeo7a\r\nUSER modeo7a 0 * :ModeO7a\r\nJOIN #otest7\r\n"; sleep 10; } | timeout 15 nc localhost $PORT > /dev/null 2>&1 &
sleep 2
{ printf "PASS $PASSWORD\r\nNICK modeo7b\r\nUSER modeo7b 0 * :ModeO7b\r\nJOIN #otest7\r\nMODE #otest7 +o modeo7a\r\n"; sleep 1; } | timeout 3 nc localhost $PORT > /tmp/ult_modeo7.log 2>&1
[ -f /tmp/ult_modeo7.log ] && grep -q "482" /tmp/ult_modeo7.log && print_test "Non-op cannot give +o (482)" "PASS" || print_test "Non-op cannot give +o (482)" "FAIL"

# ============================================================================
#                    PHASE 11: MODE +l (User Limit)
# ============================================================================

print_section "PHASE 11: MODE +l Tests"

# Test 11.1: Set +l mode
{ printf "PASS $PASSWORD\r\nNICK model1\r\nUSER model1 0 * :ModeL1\r\nJOIN #ltest\r\nMODE #ltest +l 5\r\nMODE #ltest\r\n"; sleep 1; } | timeout 3 nc localhost $PORT > /tmp/ult_model1.log 2>&1
[ -f /tmp/ult_model1.log ] && grep -q "324.*+.*l" /tmp/ult_model1.log && print_test "Set MODE +l" "PASS" || print_test "Set MODE +l" "FAIL"

# Test 11.2: Cannot join when limit reached
{ printf "PASS $PASSWORD\r\nNICK model2a\r\nUSER model2a 0 * :ModeL2a\r\nJOIN #ltest2\r\nMODE #ltest2 +l 1\r\n"; sleep 10; } | timeout 15 nc localhost $PORT > /dev/null 2>&1 &
sleep 2
{ printf "PASS $PASSWORD\r\nNICK model2b\r\nUSER model2b 0 * :ModeL2b\r\nJOIN #ltest2\r\n"; sleep 1; } | timeout 3 nc localhost $PORT > /tmp/ult_model2.log 2>&1
[ -f /tmp/ult_model2.log ] && grep -q "471" /tmp/ult_model2.log && print_test "Cannot join when limit reached (471)" "PASS" || print_test "Cannot join when limit reached (471)" "FAIL"

# Test 11.3: Can join after someone leaves
{ printf "PASS $PASSWORD\r\nNICK model3a\r\nUSER model3a 0 * :ModeL3a\r\nJOIN #ltest3\r\nMODE #ltest3 +l 2\r\n"; sleep 15; } | timeout 20 nc localhost $PORT > /dev/null 2>&1 &
sleep 2
{ printf "PASS $PASSWORD\r\nNICK model3b\r\nUSER model3b 0 * :ModeL3b\r\nJOIN #ltest3\r\n"; sleep 1; } | timeout 3 nc localhost $PORT > /dev/null 2>&1 &
sleep 1
{ printf "PART #ltest3\r\n"; sleep 1; } | timeout 3 nc localhost $PORT > /dev/null 2>&1
sleep 1
{ printf "PASS $PASSWORD\r\nNICK model3c\r\nUSER model3c 0 * :ModeL3c\r\nJOIN #ltest3\r\n"; sleep 1; } | timeout 3 nc localhost $PORT > /tmp/ult_model3.log 2>&1
[ -f /tmp/ult_model3.log ] && grep -q "JOIN" /tmp/ult_model3.log && print_test "Can join after someone leaves" "PASS" || print_test "Can join after someone leaves" "FAIL"

# Test 11.4: Remove -l mode
{ printf "PASS $PASSWORD\r\nNICK model4\r\nUSER model4 0 * :ModeL4\r\nJOIN #ltest4\r\nMODE #ltest4 +l 3\r\nMODE #ltest4 -l\r\nMODE #ltest4\r\n"; sleep 1; } | timeout 3 nc localhost $PORT > /tmp/ult_model4.log 2>&1
[ -f /tmp/ult_model4.log ] && ! grep -q "324.*+.*l" /tmp/ult_model4.log && print_test "Remove MODE -l" "PASS" || print_test "Remove MODE -l" "FAIL"

# Test 11.5: MODE +l without limit
{ printf "PASS $PASSWORD\r\nNICK model5\r\nUSER model5 0 * :ModeL5\r\nJOIN #ltest5\r\nMODE #ltest5 +l\r\n"; sleep 1; } | timeout 3 nc localhost $PORT > /tmp/ult_model5.log 2>&1
[ -f /tmp/ult_model5.log ] && grep -q "461" /tmp/ult_model5.log && print_test "MODE +l without limit (461)" "PASS" || print_test "MODE +l without limit (461)" "FAIL"

# Test 11.6: MODE +l with invalid limit
{ printf "PASS $PASSWORD\r\nNICK model6\r\nUSER model6 0 * :ModeL6\r\nJOIN #ltest6\r\nMODE #ltest6 +l 0\r\n"; sleep 1; } | timeout 3 nc localhost $PORT > /tmp/ult_model6.log 2>&1
[ -f /tmp/ult_model6.log ] && grep -q "461" /tmp/ult_model6.log && print_test "MODE +l with invalid limit (461)" "PASS" || print_test "MODE +l with invalid limit (461)" "FAIL"

# Test 11.7: Non-op cannot set +l
{ printf "PASS $PASSWORD\r\nNICK model7a\r\nUSER model7a 0 * :ModeL7a\r\nJOIN #ltest7\r\n"; sleep 10; } | timeout 15 nc localhost $PORT > /dev/null 2>&1 &
sleep 2
{ printf "PASS $PASSWORD\r\nNICK model7b\r\nUSER model7b 0 * :ModeL7b\r\nJOIN #ltest7\r\nMODE #ltest7 +l 5\r\n"; sleep 1; } | timeout 3 nc localhost $PORT > /tmp/ult_model7.log 2>&1
[ -f /tmp/ult_model7.log ] && grep -q "482" /tmp/ult_model7.log && print_test "Non-op cannot set +l (482)" "PASS" || print_test "Non-op cannot set +l (482)" "FAIL"

# ============================================================================
#                    PHASE 12: EDGE CASES
# ============================================================================

print_section "PHASE 12: Edge Cases & Stress Tests"

# Test 12.1: Very long message
LONG_MSG=$(python3 -c "print('A' * 1000)")
{ printf "PASS $PASSWORD\r\nNICK edge1\r\nUSER edge1 0 * :Edge1\r\nJOIN #edge\r\nPRIVMSG #edge :$LONG_MSG\r\n"; sleep 1; } | timeout 3 nc localhost $PORT > /tmp/ult_edge1.log 2>&1
ps -p $SERVER_PID > /dev/null && print_test "Very long message (no crash)" "PASS" || print_test "Very long message (no crash)" "FAIL"

# Test 12.2: Many rapid commands
{ printf "PASS $PASSWORD\r\nNICK edge2\r\nUSER edge2 0 * :Edge2\r\nJOIN #edge2\r\n"; for i in {1..100}; do echo "PRIVMSG #edge2 :Msg$i"; done; sleep 1; } | timeout 3 nc localhost $PORT > /dev/null 2>&1
ps -p $SERVER_PID > /dev/null && print_test "100 rapid commands (no crash)" "PASS" || print_test "100 rapid commands (no crash)" "FAIL"

# Test 12.3: Empty command
{ printf "PASS $PASSWORD\r\nNICK edge3\r\nUSER edge3 0 * :Edge3\r\n\r\n\r\n"; sleep 1; } | timeout 3 nc localhost $PORT > /tmp/ult_edge3.log 2>&1
ps -p $SERVER_PID > /dev/null && print_test "Empty lines (no crash)" "PASS" || print_test "Empty lines (no crash)" "FAIL"

# Test 12.4: Binary data
dd if=/dev/urandom bs=256 count=1 2>/dev/null | timeout 2 nc localhost $PORT > /dev/null 2>&1
sleep 1
ps -p $SERVER_PID > /dev/null && print_test "Random binary data (no crash)" "PASS" || print_test "Random binary data (no crash)" "FAIL"

# Test 12.5: Rapid connect/disconnect
for i in {1..10}; do
    { printf "PASS $PASSWORD\r\n"; sleep 0.1; } | timeout 1 nc localhost $PORT > /dev/null 2>&1 &
done
sleep 2
ps -p $SERVER_PID > /dev/null && print_test "10 rapid connect/disconnect (no crash)" "PASS" || print_test "10 rapid connect/disconnect (no crash)" "FAIL"

# ============================================================================
#                    PHASE 13: MEMORY LEAKS
# ============================================================================

print_section "PHASE 13: Memory Leak Detection (Valgrind)"

stop_server
sleep 1

echo "Recompiling with debug symbols..."
make CXXFLAGS="-Wall -Wextra -Werror -std=c++98 -g -I./includes" re > /dev/null 2>&1

echo "Running server with valgrind..."
valgrind --leak-check=full --error-exitcode=1 --log-file=/tmp/ult_valgrind.log \
    ./ircserv $PORT $PASSWORD > /tmp/ult_valgrind_server.log 2>&1 &
SERVER_PID=$!
sleep 2

# Run various tests
for i in {1..20}; do
    { printf "PASS $PASSWORD\r\nNICK leak$i\r\nUSER leak$i 0 * :Leak$i\r\nJOIN #leak\r\nPRIVMSG #leak :Test\r\nPART #leak\r\n"; sleep 1; } | timeout 3 nc localhost $PORT > /dev/null 2>&1 &
    [ $(($i % 5)) -eq 0 ] && sleep 1
done
sleep 3

# Modes stress test
{ printf "PASS $PASSWORD\r\nNICK modestress\r\nUSER modestress 0 * :ModeStress\r\nJOIN #modestress\r\n"; for i in {1..20}; do echo "MODE #modestress +k pass$i"; echo "MODE #modestress -k"; done; sleep 1; } | timeout 5 nc localhost $PORT > /dev/null 2>&1

sleep 2

kill -INT $SERVER_PID 2>/dev/null
wait $SERVER_PID 2>/dev/null
VALGRIND_EXIT=$?

if [ $VALGRIND_EXIT -eq 0 ]; then
    if grep -q "no leaks are possible\|ERROR SUMMARY: 0 errors" /tmp/ult_valgrind.log; then
        print_test "Memory leaks (valgrind)" "PASS"
    else
        print_test "Memory leaks (valgrind)" "FAIL"
        echo "  Leak summary:"
        grep "definitely lost\|indirectly lost" /tmp/ult_valgrind.log | head -3
    fi
else
    print_test "Memory leaks (valgrind)" "FAIL"
    echo "  Valgrind exited with error"
fi

# Recompile normal version
make re > /dev/null 2>&1

# ============================================================================
#                          FINAL SUMMARY
# ============================================================================

echo ""
echo -e "${MAGENTA}╔══════════════════════════════════════════════════════════════╗${RESET}"
echo -e "${MAGENTA}║                      FINAL SUMMARY                           ║${RESET}"
echo -e "${MAGENTA}╠══════════════════════════════════════════════════════════════╣${RESET}"
echo -e "${MAGENTA}║${RESET} Total Tests:     $TOTAL"
echo -e "${MAGENTA}║${RESET} ${GREEN}Passed:${RESET}          $PASSED"
echo -e "${MAGENTA}║${RESET} ${RED}Failed:${RESET}          $FAILED"
PERCENTAGE=$((PASSED * 100 / TOTAL))
echo -e "${MAGENTA}║${RESET} Success Rate:    $PERCENTAGE%"
echo -e "${MAGENTA}╚══════════════════════════════════════════════════════════════╝${RESET}"

if [ $FAILED -eq 0 ]; then
    echo -e "\n${GREEN}╔══════════════════════════════════════════════════════════════╗${RESET}"
    echo -e "${GREEN}║                                                              ║${RESET}"
    echo -e "${GREEN}║         ✓✓✓ ALL TESTS PASSED! ✓✓✓                           ║${RESET}"
    echo -e "${GREEN}║                                                              ║${RESET}"
    echo -e "${GREEN}║     Your IRC server is ready for evaluation! 🎉             ║${RESET}"
    echo -e "${GREEN}║                                                              ║${RESET}"
    echo -e "${GREEN}╚══════════════════════════════════════════════════════════════╝${RESET}\n"
    exit 0
else
    echo -e "\n${RED}╔══════════════════════════════════════════════════════════════╗${RESET}"
    echo -e "${RED}║                                                              ║${RESET}"
    echo -e "${RED}║     ✗ SOME TESTS FAILED ✗                                   ║${RESET}"
    echo -e "${RED}║                                                              ║${RESET}"
    echo -e "${RED}║     Review failed tests and fix before evaluation           ║${RESET}"
    echo -e "${RED}║                                                              ║${RESET}"
    echo -e "${RED}╚══════════════════════════════════════════════════════════════╝${RESET}\n"
    echo "Detailed logs available in /tmp/ult_*.log"
    exit 1
fi