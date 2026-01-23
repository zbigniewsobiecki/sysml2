// Test file with intentional errors for error recovery testing

package Errors {

    // Missing semicolon after feature
    class MissingSemicolon {
        feature x : Integer
        feature y : Integer;
    }

    // Undefined type reference
    class UndefinedType {
        feature value : NonExistentType;
    }

    // Duplicate name
    class Duplicate {
        feature name : String;
        feature name : Integer;  // Duplicate
    }

    // Missing closing brace
    // class Unclosed {
    //     feature x : Integer;

    // Typo in type name
    class TypeTypo {
        feature engine : Engin;  // Did you mean 'Engine'?
    }
}
