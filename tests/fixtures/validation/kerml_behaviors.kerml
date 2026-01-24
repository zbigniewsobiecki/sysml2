// KerML Behavior Definitions Test
package KerMLBehaviors {
    behavior BaseBehavior;
    behavior DerivedBehavior :> BaseBehavior;

    interaction MyInteraction;

    function square;
    function computeSum;

    predicate isPositive;
    predicate isNegative;

    abstract behavior AbstractBehavior;
    abstract function AbstractFunction;
}
