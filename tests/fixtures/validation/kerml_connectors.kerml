// KerML Connector Elements Test
package KerMLConnectors {
    class Component {
        feature port1;
        feature port2;
        feature out1;
        feature in1;
    }

    class System {
        feature source;
        feature target;

        connector link from source to target;
        binding b of source = target;
        succession s first source then target;
    }
}
