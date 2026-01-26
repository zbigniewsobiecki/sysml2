// Test fixture for E3003: Valid import of existing namespace
package LibraryPkg {
    part def Component;
}

package TestPackage {
    import LibraryPkg::*;  // Valid: LibraryPkg exists

    part myComponent : Component;
}
