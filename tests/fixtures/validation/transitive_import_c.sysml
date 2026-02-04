// Transitive import test — File C (uses Facade, expects to see Inner types)
package App {
    import Facade::*;
    part myGadget : Gadget;
    part myWidget : Widget;
}
