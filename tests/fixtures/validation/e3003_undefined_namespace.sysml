// Test fixture for E3003: Import of undefined namespace
package TestPackage {
    import NonExistent::*;  // Error: namespace 'NonExistent' not found

    part def Vehicle;
}
