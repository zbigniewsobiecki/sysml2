package TypedUsages {
    part def Engine;
    part def Wheel;

    part vehicle {
        part engine : Engine;
        part frontLeft : Wheel;
        part frontRight : Wheel;
    }
}
