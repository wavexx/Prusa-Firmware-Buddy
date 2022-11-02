#include "PrusaGcodeSuite.hpp"
#include "../../Marlin/src/feature/prusa/measure_axis.h"
#include "selftest_sub_state.hpp"
#include "crash_recovery_type.hpp"
#include "client_fsm_types.h"
#include "client_response.hpp"
#include "marlin_server.hpp"
#include "../../lib/Marlin/Marlin/src/gcode/parser.h"
#include "selftest_axis.h"

#if ENABLED(CRASH_RECOVERY)
static bool axis_length_ok(AxisEnum axis, float length) {
    switch (axis) {
    case X_AXIS:
        return ((length <= selftest::Config_XAxis.length_max) && (length >= selftest::Config_XAxis.length_min));
    case Y_AXIS:
        return ((length <= selftest::Config_YAxis.length_max) && (length >= selftest::Config_YAxis.length_min));
    default:;
    }
    return false;
}

static SelftestSubtestState_t axis_length_ok_fsm(AxisEnum axis, float length) {
    return axis_length_ok(axis, length) ? SelftestSubtestState_t::ok : SelftestSubtestState_t::not_good;
}
#endif

void PrusaGcodeSuite::G163() {
#if ENABLED(CRASH_RECOVERY)
    Crash_recovery_fsm cr_fsm(SelftestSubtestState_t::running, SelftestSubtestState_t::undef);
    fsm_change(ClientFSM::CrashRecovery, PhasesCrashRecovery::check_X, cr_fsm.Serialize());
    bool do_x = parser.seen('X');
    bool do_y = parser.seen('Y');
    if (!do_x && !do_y)
        return;

    Measure_axis ma(do_x, do_y, { true, true });
    if (parser.seen('S')) {
        int value = parser.value_int();
        xy_long_t sens = { value, value };
        ma.set_sensitivity(sens);
    }
    if (parser.seen('P')) {
        int value = parser.value_int();
        xy_long_t per = { value, value };
        ma.set_period(per);
    }
    ma.start();

    if (do_y) {
        while (ma.state() != Measure_axis::BACK_Y) {
            idle(true);
            ma.loop();
        }
        cr_fsm.set(axis_length_ok_fsm(X_AXIS, ma.length().x), SelftestSubtestState_t::running);
        fsm_change(ClientFSM::CrashRecovery, PhasesCrashRecovery::check_Y, cr_fsm.Serialize());
    }

    while (ma.state() != Measure_axis::FINISH) {
        idle(true);
        ma.loop(); /// loop must be after idle so the length is processed here sooner than in marlin_server
    }

    if (do_x)
        SERIAL_ECHOLNPGM("X length: ", ma.length().x);
    if (do_y)
        SERIAL_ECHOLNPGM("Y length: ", ma.length().y);

    set_length(ma.length());
    cr_fsm.set(axis_length_ok_fsm(X_AXIS, ma.length().x), axis_length_ok_fsm(Y_AXIS, ma.length().y));
    fsm_change(ClientFSM::CrashRecovery, PhasesCrashRecovery::check_Y, cr_fsm.Serialize());
#endif
}
