package Requirements {
    requirement def PerformanceReq {
        doc /* Performance must meet spec */
    }

    constraint def SpeedConstraint {
        doc /* Speed must be positive */
    }

    requirement safetyReq : PerformanceReq;
}
