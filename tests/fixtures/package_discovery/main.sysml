/*
 * Test fixture: Main file importing package from _index.sysml
 *
 * This tests that import SystemBehavior::* can find the package
 * even though it's defined in behavior/_index.sysml, not SystemBehavior.sysml.
 */
package AppModel {
    import SystemBehavior::*;

    part system {
        // Use action from imported package
        perform action doWork : DoSomething;
    }
}
