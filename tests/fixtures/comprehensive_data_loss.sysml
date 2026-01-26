/**
 * Comprehensive Data Loss Test Fixture
 *
 * This file tests many SysML v2 features to ensure no data loss
 * during parse -> write -> parse round trips.
 *
 * Features tested:
 * - Package structure (public, private, library)
 * - Part definitions and usages with multiplicities
 * - Port definitions (conjugated, ref, nested)
 * - Action definitions with control flow
 * - State definitions with transitions
 * - Requirement definitions with constraints
 * - Metadata definitions and annotations
 * - Connections and flows
 * - Enumerations with values
 * - Aliases
 * - Various visibility modifiers
 * - Documentation comments
 * - Specialization, subsetting, redefinition
 * - Shorthand notations
 */
package ComprehensiveDataLossTest {
    private import SI::*;
    private import ScalarValues::*;

    /* ========================================
     * Part 1: Basic Definitions and Usages
     * ======================================== */

    /** Documentation for abstract part definition */
    public abstract part def Vehicle {
        doc /* Represents a generic vehicle */

        attribute mass : Real;
        attribute maxSpeed : Real = 100.0;
        constant attribute wheels : Integer = 4;
        derived attribute ratio :> mass;

        part engine : Engine[1];
        part passengers : Person[0..5];

        port fuelPort : FuelPort;
        ref port controlPort : ~ControlPort;
    }

    private part def Engine {
        attribute power : Real;
        attribute displacement : Real;
        ref cylinders : Integer;
    }

    part def Person;

    /** Port definition with nested ports */
    port def FuelPort {
        in item fuel : Fuel;
        out attribute pressure : Real;
        port subPort {
            in x;
        }
    }

    port def ControlPort {
        in cmd : Command;
        out status : Status;
    }

    port def Command;
    port def Status;
    item def Fuel;

    part myCar : Vehicle {
        doc /* A specific car instance */
        :>> mass = 1500.0;
        :>> maxSpeed = 200.0;

        part :>> engine {
            :>> power = 150.0;
            :>> displacement = 2.0;
        }

        part driver :> passengers {
            @Important;
        }
    }

    /* ========================================
     * Part 2: Actions and Control Flow
     * ======================================== */

    action def StartVehicle {
        in vehicle : Vehicle;
        out result : Boolean;
    }

    action def DriveAction {
        in destination : String;
        in vehicle : Vehicle;

        action checkFuel {
            out fuelOk : Boolean;
        }

        action drive;
        action refuel;
    }

    action driveToWork : DriveAction {
        in :>> destination = "work";
        in :>> vehicle = myCar;
    }

    action complexFlow {
        action step1;
        action step2;
        action parallel1;
        action parallel2;
    }

    /* ========================================
     * Part 3: States and Transitions
     * ======================================== */

    attribute def PowerOn;
    attribute def PowerOff;
    attribute def Timeout;

    state def VehicleState {
        entry action initialize;
        do action monitor;
        exit action cleanup;

        state Off {
            doc /* Vehicle is powered off */
        }
        state On {
            state Idle;
            state Moving {
                state Accelerating;
                state Cruising;
                state Braking;
            }
        }
    }

    state vehicleLifecycle : VehicleState;

    state parallelStates parallel {
        state region1 {
            state s1a;
            state s1b;
        }
        state region2 {
            state s2a;
            state s2b;
        }
    }

    /* ========================================
     * Part 4: Requirements and Constraints
     * ======================================== */

    constraint def SpeedLimit {
        in speed : Real;
        in maxAllowed : Real;
        speed <= maxAllowed
    }

    constraint def WeightLimit {
        in weight : Real;
        in maxWeight : Real = 5000.0;
    }

    requirement def VehicleSafetyRequirement {
        doc /* Safety requirements for vehicles */
        subject vehicle : Vehicle;

        assume constraint massConstraint : WeightLimit {
            in :>> weight = vehicle.mass;
        }

        require constraint speedConstraint : SpeedLimit {
            in :>> speed = vehicle.maxSpeed;
            in :>> maxAllowed = 250.0;
        }

        requirement subReq1 {
            doc /* Sub-requirement 1 */
        }

        requirement subReq2 {
            doc /* Sub-requirement 2 */
        }
    }

    requirement safetyReq : VehicleSafetyRequirement {
        subject :>> vehicle = myCar;
    }

    satisfy safetyReq by myCar;
    assert satisfy safetyReq by myCar;

    /* ========================================
     * Part 5: Connections and Flows
     * ======================================== */

    part def SystemA {
        port outPort : FuelPort;
    }

    part def SystemB {
        port inPort : ~FuelPort;
    }

    connection def FuelConnection {
        end source : SystemA;
        end target : SystemB;
    }

    part system {
        part a : SystemA;
        part b : SystemB;

        connect a.outPort to b.inPort;

        connection fuelLine : FuelConnection connect (a, b);
    }

    flow def FuelFlow {
        end source;
        end target;
        ref item flowing : Fuel;
    }

    flow fuelTransfer : FuelFlow from system.a.outPort to system.b.inPort;

    interface def SystemInterface {
        end [1] sysA : SystemA;
        end [*] sysB : SystemB;
    }

    /* ========================================
     * Part 6: Metadata and Annotations
     * ======================================== */

    metadata def Important {
        ref :>> annotatedElement : SysML::Usage;
        attribute priority : Integer = 1;
    }

    metadata def Deprecated {
        attribute reason : String;
    }

    metadata def CustomTag;

    #Important part criticalComponent {
        @Deprecated {
            reason = "Use newComponent instead";
        }
    }

    #CustomTag #Important part taggedPart;

    ref annotatedRef {
        metadata Important {
            priority = 10;
        }
    }

    /* ========================================
     * Part 7: Enumerations
     * ======================================== */

    enum def Color {
        red;
        green;
        blue;
        enum custom;
    }

    enum def Priority :> Integer {
        low : Priority = 1;
        medium : Priority = 2;
        high : Priority = 3;
        critical : Priority = 10;
    }

    attribute myColor : Color = Color::blue;
    attribute myPriority : Priority = Priority::high;

    /* ========================================
     * Part 8: Aliases and Visibility
     * ======================================== */

    library package TypeLibrary {
        public part def PublicType;
        private part def PrivateType;
        protected part def ProtectedType;

        alias PublicAlias for PublicType;
        alias PrivateAlias for PrivateType;
    }

    alias VehicleAlias for Vehicle;
    alias EngineAlias for Engine;

    /* ========================================
     * Part 9: Complex Nesting and References
     * ======================================== */

    part def ComplexSystem {
        package SubPackage {
            part def InnerDef;
        }

        part inner : SubPackage::InnerDef;

        part nested {
            part level1 {
                part level2 {
                    part level3;
                }
            }
        }

        ref external : Vehicle;
    }

    part complexInstance : ComplexSystem {
        part :>> nested {
            part :>> level1 {
                part :>> level2 {
                    part :>> level3 {
                        attribute deepAttr : String;
                    }
                }
            }
        }
    }

    /* ========================================
     * Part 10: Specialization Chains
     * ======================================== */

    part def Base {
        attribute baseAttr : String;
        part basePart;
    }

    part def Derived1 :> Base {
        attribute derived1Attr : Integer;
        part derived1Part :> basePart;
    }

    part def Derived2 :> Derived1 {
        attribute derived2Attr : Real;
        part derived2Part redefines derived1Part;
    }

    part instance1 : Base;
    part instance2 : Derived1;
    part instance3 : Derived2;

    /* ========================================
     * Part 11: Item Definitions with Flows
     * ======================================== */

    item def DataPacket {
        attribute payload : String;
        attribute timestamp : Integer;
        attribute size : Integer;
    }

    item def ControlSignal {
        enum def SignalType {
            start;
            stop;
            pause;
            resume;
        }
        attribute signalType : SignalType;
    }

    action dataProcessor {
        in item packet : DataPacket;
        out item result : DataPacket;

        action process {
            in item :>> packet;
            out item processed : DataPacket;
        }

        flow process.processed to result;
    }

    /* ========================================
     * Part 12: Occurrence Definitions
     * ======================================== */

    occurrence def Event1 {
        attribute timestamp : Integer;
    }
    occurrence def Event2;

    occurrence occ1 : Event1;
    occurrence occ2 : Event2;
    ref occurrence occ3 : Event1;

    /* ========================================
     * Part 13: Calculation and Analysis
     * ======================================== */

    calc def TotalMass {
        in vehicleMass : Real;
        in passengerCount : Integer;
        in avgPassengerMass : Real = 75.0;
        return : Real = vehicleMass + passengerCount * avgPassengerMass;
    }

    calc totalCarMass : TotalMass {
        in :>> vehicleMass = myCar.mass;
        in :>> passengerCount = 4;
    }

    analysis def FuelEfficiencyAnalysis {
        subject vehicle : Vehicle;
        objective {
            doc /* Analyze fuel efficiency */
        }
        return efficiency : Real;
    }

    /* ========================================
     * Part 14: Use Cases and Scenarios
     * ======================================== */

    use case def DriveVehicle {
        subject driver : Person;
        objective {
            doc /* Drive vehicle from A to B */
        }

        actor user : Person;

        include use case startEngine;
        include use case navigate;
        include use case park;
    }

    use case driveToStore : DriveVehicle {
        subject :>> driver;
    }

    /* ========================================
     * Part 15: Verification and Validation
     * ======================================== */

    verification def SafetyVerification {
        subject vehicle : Vehicle;
        objective {
            verify safetyReq;
        }

        action testBrakes;
        action testSeatbelts;
        action testAirbags;
    }

    part verificationContext {
        verification safetyTest : SafetyVerification {
            subject :>> vehicle = myCar;
        }
    }

    /* ========================================
     * Part 16: Views and Viewpoints
     * ======================================== */

    viewpoint def EngineerViewpoint {
        doc /* Viewpoint for engineers */
        stakeholder engineer;
    }

    rendering def DiagramRendering;

    view def SystemOverview {
        viewpoint vp : EngineerViewpoint;
        expose Vehicle::*;
        expose Engine::*;
        render rendering r : DiagramRendering;
    }

    view systemView : SystemOverview;

    /* ========================================
     * Part 17: Allocation
     * ======================================== */

    allocation def FunctionToComponent {
        end function;
        end component;
    }

    allocation driveAllocation : FunctionToComponent
        allocate DriveAction to myCar;

    /* ========================================
     * Part 18: Concern and Stakeholder
     * ======================================== */

    concern def SafetyConcern {
        doc /* Concern about system safety */
        stakeholder safetyEngineer;
    }

    concern safetyConcern : SafetyConcern;

    /* ========================================
     * Part 19: Expression Bodies
     * ======================================== */

    attribute computed : Real = 2 * 3 + 4 / 2 - 1;
    attribute conditional : Boolean = true and false or not true;
    attribute comparison : Boolean = 10 > 5 and 3 <= 3;

    /* ========================================
     * Part 20: Trivia Preservation Tests
     * ======================================== */

    // Single line comment before element
    part def WithSingleLineComment {
        attribute x : Integer;
    }

    part def First;

    part def Second;


    part def Third;

    part def MixedTrivia {

        attribute a : Integer;

        attribute b : String;
    }
}
