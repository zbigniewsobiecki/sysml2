/*
 * Test fixture: Package in file where filename != package name
 *
 * This tests that sysml2 can discover packages by parsing files,
 * not just by matching filenames.
 */
package SystemBehavior {
    action def DoSomething {
        in input;
        out output;
    }

    action def ProcessData :> DoSomething;
}
