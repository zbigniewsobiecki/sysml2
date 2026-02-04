package Pkg {
    part def User;
    part def System;
    part def PM;
    use case def UC {
        subject sys:System;
        actor u:User;
        // subject note
        // actor note
    }
    concern def C {
        stakeholder pm:PM;
        // stakeholder note
    }
}
