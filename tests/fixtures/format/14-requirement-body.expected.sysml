package ReqBodyTest {
    requirement def R {
        subject s:Type;
        assume constraint a{x>0}
        require constraint r:C{y<10}
    }
}
