// KerML Feature Elements Test
package KerMLFeatures {
    class Container {
        feature count;
        feature items[*];

        step processItem;

        expr computeTotal;

        bool isEmpty;

        inv true positiveCount;

        in feature input;
        out feature output;
    }

    class Item;

    namespace FeatureNS {
        feature standalone;
    }
}
