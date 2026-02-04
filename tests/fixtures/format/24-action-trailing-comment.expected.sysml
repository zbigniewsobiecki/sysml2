state def S {
    entry init;
    do monitor;
    exit cleanup;
    // entry comment
    // do comment
    // exit comment
}

action def A {
    action a;
    action b;
    // succession comment
    action c;
    first a then b;
}
