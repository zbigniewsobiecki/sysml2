/*
 * SysML v2 Parser - AST Utilities
 *
 * SPDX-License-Identifier: MIT
 */

#include "sysml2/ast.h"

/*
 * Mapping of node kinds to JSON type strings.
 *
 * These match the plan's JSON Type Mapping:
 * - Definitions use "*Def" suffix
 * - Usages use bare names (Part, Port, Action, etc.)
 * - Relationships use their semantic names
 */
const char *sysml_kind_to_json_type(SysmlNodeKind kind) {
    switch (kind) {
        /* Imports */
        case SYSML_KIND_IMPORT:           return "Import";
        case SYSML_KIND_IMPORT_ALL:       return "ImportAll";
        case SYSML_KIND_IMPORT_RECURSIVE: return "ImportRecursive";

        /* Packages */
        case SYSML_KIND_PACKAGE:         return "Package";
        case SYSML_KIND_LIBRARY_PACKAGE: return "LibraryPackage";

        /* Definitions */
        case SYSML_KIND_ATTRIBUTE_DEF:   return "AttributeDef";
        case SYSML_KIND_ENUMERATION_DEF: return "EnumerationDef";
        case SYSML_KIND_OCCURRENCE_DEF:  return "OccurrenceDef";
        case SYSML_KIND_ITEM_DEF:        return "ItemDef";
        case SYSML_KIND_PART_DEF:        return "PartDef";
        case SYSML_KIND_CONNECTION_DEF:  return "ConnectionDef";
        case SYSML_KIND_FLOW_DEF:        return "FlowDef";
        case SYSML_KIND_INTERFACE_DEF:   return "InterfaceDef";
        case SYSML_KIND_PORT_DEF:        return "PortDef";
        case SYSML_KIND_ALLOCATION_DEF:  return "AllocationDef";
        case SYSML_KIND_ACTION_DEF:      return "ActionDef";
        case SYSML_KIND_STATE_DEF:       return "StateDef";
        case SYSML_KIND_CONSTRAINT_DEF:  return "ConstraintDef";
        case SYSML_KIND_REQUIREMENT_DEF: return "RequirementDef";
        case SYSML_KIND_CONCERN_DEF:     return "ConcernDef";
        case SYSML_KIND_CALC_DEF:        return "CalcDef";
        case SYSML_KIND_CASE_DEF:        return "CaseDef";
        case SYSML_KIND_ANALYSIS_DEF:    return "AnalysisDef";
        case SYSML_KIND_VERIFICATION_DEF: return "VerificationDef";
        case SYSML_KIND_USE_CASE_DEF:    return "UseCaseDef";
        case SYSML_KIND_VIEW_DEF:        return "ViewDef";
        case SYSML_KIND_VIEWPOINT_DEF:   return "ViewpointDef";
        case SYSML_KIND_RENDERING_DEF:   return "RenderingDef";
        case SYSML_KIND_METADATA_DEF:    return "MetadataDef";
        case SYSML_KIND_DATATYPE:        return "DataType";

        /* KerML Definitions */
        case SYSML_KIND_NAMESPACE:       return "Namespace";
        case SYSML_KIND_TYPE:            return "Type";
        case SYSML_KIND_CLASSIFIER:      return "Classifier";
        case SYSML_KIND_CLASS:           return "Class";
        case SYSML_KIND_STRUCTURE:       return "Structure";
        case SYSML_KIND_METACLASS:       return "Metaclass";
        case SYSML_KIND_ASSOCIATION:     return "Association";
        case SYSML_KIND_ASSOC_STRUCT:    return "AssociationStructure";
        case SYSML_KIND_INTERACTION:     return "Interaction";
        case SYSML_KIND_BEHAVIOR:        return "Behavior";
        case SYSML_KIND_FUNCTION:        return "Function";
        case SYSML_KIND_PREDICATE:       return "Predicate";
        case SYSML_KIND_MULTIPLICITY_DEF: return "Multiplicity";

        /* Usages */
        case SYSML_KIND_ATTRIBUTE_USAGE:   return "Attribute";
        case SYSML_KIND_ENUMERATION_USAGE: return "Enumeration";
        case SYSML_KIND_OCCURRENCE_USAGE:  return "Occurrence";
        case SYSML_KIND_ITEM_USAGE:        return "Item";
        case SYSML_KIND_PART_USAGE:        return "Part";
        case SYSML_KIND_CONNECTION_USAGE:  return "Connection";
        case SYSML_KIND_FLOW_USAGE:        return "Flow";
        case SYSML_KIND_INTERFACE_USAGE:   return "Interface";
        case SYSML_KIND_PORT_USAGE:        return "Port";
        case SYSML_KIND_ALLOCATION_USAGE:  return "Allocation";
        case SYSML_KIND_ACTION_USAGE:      return "Action";
        case SYSML_KIND_STATE_USAGE:       return "State";
        case SYSML_KIND_CONSTRAINT_USAGE:  return "Constraint";
        case SYSML_KIND_REQUIREMENT_USAGE: return "Requirement";
        case SYSML_KIND_CONCERN_USAGE:     return "Concern";
        case SYSML_KIND_CALC_USAGE:        return "Calc";
        case SYSML_KIND_CASE_USAGE:        return "Case";
        case SYSML_KIND_ANALYSIS_USAGE:    return "Analysis";
        case SYSML_KIND_VERIFICATION_USAGE: return "Verification";
        case SYSML_KIND_USE_CASE_USAGE:    return "UseCase";
        case SYSML_KIND_VIEW_USAGE:        return "View";
        case SYSML_KIND_VIEWPOINT_USAGE:   return "Viewpoint";
        case SYSML_KIND_RENDERING_USAGE:   return "Rendering";
        case SYSML_KIND_REFERENCE_USAGE:   return "Reference";
        case SYSML_KIND_EVENT_USAGE:       return "Event";
        case SYSML_KIND_PORTION_USAGE:     return "Portion";
        case SYSML_KIND_SUBJECT_USAGE:     return "Subject";
        case SYSML_KIND_ACTOR_USAGE:       return "Actor";
        case SYSML_KIND_STAKEHOLDER_USAGE: return "Stakeholder";
        case SYSML_KIND_MESSAGE_USAGE:     return "Message";

        /* KerML Features */
        case SYSML_KIND_FEATURE:           return "Feature";
        case SYSML_KIND_STEP:              return "Step";
        case SYSML_KIND_EXPRESSION:        return "Expression";
        case SYSML_KIND_BOOL_EXPRESSION:   return "BooleanExpression";
        case SYSML_KIND_INVARIANT:         return "Invariant";
        case SYSML_KIND_CONNECTOR:         return "Connector";
        case SYSML_KIND_BINDING_CONNECTOR: return "BindingConnector";
        case SYSML_KIND_SUCCESSION:        return "Succession";
        case SYSML_KIND_KERML_FLOW:        return "Flow";
        case SYSML_KIND_SUCCESSION_FLOW:   return "SuccessionFlow";

        /* Relationships */
        case SYSML_KIND_REL_CONNECTION:  return "Connection";
        case SYSML_KIND_REL_FLOW:        return "Flow";
        case SYSML_KIND_REL_ALLOCATION:  return "Allocation";
        case SYSML_KIND_REL_SATISFY:     return "Satisfy";
        case SYSML_KIND_REL_VERIFY:      return "Verify";
        case SYSML_KIND_REL_TRANSITION:  return "Transition";
        case SYSML_KIND_REL_SUCCESSION:  return "Succession";
        case SYSML_KIND_REL_BIND:        return "Bind";

        /* KerML Relationships */
        case SYSML_KIND_REL_SPECIALIZATION:    return "Specialization";
        case SYSML_KIND_REL_CONJUGATION:       return "Conjugation";
        case SYSML_KIND_REL_SUBCLASSIFICATION: return "Subclassification";
        case SYSML_KIND_REL_DISJOINING:        return "Disjoining";
        case SYSML_KIND_REL_INVERTING:         return "FeatureInverting";
        case SYSML_KIND_REL_TYPING:            return "FeatureTyping";
        case SYSML_KIND_REL_SUBSETTING:        return "Subsetting";
        case SYSML_KIND_REL_REDEFINITION:      return "Redefinition";
        case SYSML_KIND_REL_FEATURING:         return "TypeFeaturing";

        case SYSML_KIND_UNKNOWN:
        default:
            return "Unknown";
    }
}

/*
 * Human-readable names for debugging and error messages
 */
const char *sysml_kind_to_string(SysmlNodeKind kind) {
    switch (kind) {
        /* Imports */
        case SYSML_KIND_IMPORT:           return "Import";
        case SYSML_KIND_IMPORT_ALL:       return "ImportAll";
        case SYSML_KIND_IMPORT_RECURSIVE: return "ImportRecursive";

        /* Packages */
        case SYSML_KIND_PACKAGE:         return "Package";
        case SYSML_KIND_LIBRARY_PACKAGE: return "LibraryPackage";

        /* Definitions */
        case SYSML_KIND_ATTRIBUTE_DEF:   return "AttributeDefinition";
        case SYSML_KIND_ENUMERATION_DEF: return "EnumerationDefinition";
        case SYSML_KIND_OCCURRENCE_DEF:  return "OccurrenceDefinition";
        case SYSML_KIND_ITEM_DEF:        return "ItemDefinition";
        case SYSML_KIND_PART_DEF:        return "PartDefinition";
        case SYSML_KIND_CONNECTION_DEF:  return "ConnectionDefinition";
        case SYSML_KIND_FLOW_DEF:        return "FlowDefinition";
        case SYSML_KIND_INTERFACE_DEF:   return "InterfaceDefinition";
        case SYSML_KIND_PORT_DEF:        return "PortDefinition";
        case SYSML_KIND_ALLOCATION_DEF:  return "AllocationDefinition";
        case SYSML_KIND_ACTION_DEF:      return "ActionDefinition";
        case SYSML_KIND_STATE_DEF:       return "StateDefinition";
        case SYSML_KIND_CONSTRAINT_DEF:  return "ConstraintDefinition";
        case SYSML_KIND_REQUIREMENT_DEF: return "RequirementDefinition";
        case SYSML_KIND_CONCERN_DEF:     return "ConcernDefinition";
        case SYSML_KIND_CALC_DEF:        return "CalcDefinition";
        case SYSML_KIND_CASE_DEF:        return "CaseDefinition";
        case SYSML_KIND_ANALYSIS_DEF:    return "AnalysisDefinition";
        case SYSML_KIND_VERIFICATION_DEF: return "VerificationDefinition";
        case SYSML_KIND_USE_CASE_DEF:    return "UseCaseDefinition";
        case SYSML_KIND_VIEW_DEF:        return "ViewDefinition";
        case SYSML_KIND_VIEWPOINT_DEF:   return "ViewpointDefinition";
        case SYSML_KIND_RENDERING_DEF:   return "RenderingDefinition";
        case SYSML_KIND_METADATA_DEF:    return "MetadataDefinition";
        case SYSML_KIND_DATATYPE:        return "DataTypeDefinition";

        /* KerML Definitions */
        case SYSML_KIND_NAMESPACE:       return "Namespace";
        case SYSML_KIND_TYPE:            return "Type";
        case SYSML_KIND_CLASSIFIER:      return "Classifier";
        case SYSML_KIND_CLASS:           return "Class";
        case SYSML_KIND_STRUCTURE:       return "Structure";
        case SYSML_KIND_METACLASS:       return "Metaclass";
        case SYSML_KIND_ASSOCIATION:     return "Association";
        case SYSML_KIND_ASSOC_STRUCT:    return "AssociationStructure";
        case SYSML_KIND_INTERACTION:     return "Interaction";
        case SYSML_KIND_BEHAVIOR:        return "Behavior";
        case SYSML_KIND_FUNCTION:        return "Function";
        case SYSML_KIND_PREDICATE:       return "Predicate";
        case SYSML_KIND_MULTIPLICITY_DEF: return "MultiplicityDefinition";

        /* Usages */
        case SYSML_KIND_ATTRIBUTE_USAGE:   return "AttributeUsage";
        case SYSML_KIND_ENUMERATION_USAGE: return "EnumerationUsage";
        case SYSML_KIND_OCCURRENCE_USAGE:  return "OccurrenceUsage";
        case SYSML_KIND_ITEM_USAGE:        return "ItemUsage";
        case SYSML_KIND_PART_USAGE:        return "PartUsage";
        case SYSML_KIND_CONNECTION_USAGE:  return "ConnectionUsage";
        case SYSML_KIND_FLOW_USAGE:        return "FlowUsage";
        case SYSML_KIND_INTERFACE_USAGE:   return "InterfaceUsage";
        case SYSML_KIND_PORT_USAGE:        return "PortUsage";
        case SYSML_KIND_ALLOCATION_USAGE:  return "AllocationUsage";
        case SYSML_KIND_ACTION_USAGE:      return "ActionUsage";
        case SYSML_KIND_STATE_USAGE:       return "StateUsage";
        case SYSML_KIND_CONSTRAINT_USAGE:  return "ConstraintUsage";
        case SYSML_KIND_REQUIREMENT_USAGE: return "RequirementUsage";
        case SYSML_KIND_CONCERN_USAGE:     return "ConcernUsage";
        case SYSML_KIND_CALC_USAGE:        return "CalcUsage";
        case SYSML_KIND_CASE_USAGE:        return "CaseUsage";
        case SYSML_KIND_ANALYSIS_USAGE:    return "AnalysisUsage";
        case SYSML_KIND_VERIFICATION_USAGE: return "VerificationUsage";
        case SYSML_KIND_USE_CASE_USAGE:    return "UseCaseUsage";
        case SYSML_KIND_VIEW_USAGE:        return "ViewUsage";
        case SYSML_KIND_VIEWPOINT_USAGE:   return "ViewpointUsage";
        case SYSML_KIND_RENDERING_USAGE:   return "RenderingUsage";
        case SYSML_KIND_REFERENCE_USAGE:   return "ReferenceUsage";
        case SYSML_KIND_EVENT_USAGE:       return "EventUsage";
        case SYSML_KIND_PORTION_USAGE:     return "PortionUsage";
        case SYSML_KIND_SUBJECT_USAGE:     return "SubjectUsage";
        case SYSML_KIND_ACTOR_USAGE:       return "ActorUsage";
        case SYSML_KIND_STAKEHOLDER_USAGE: return "StakeholderUsage";
        case SYSML_KIND_MESSAGE_USAGE:     return "MessageUsage";

        /* KerML Features */
        case SYSML_KIND_FEATURE:           return "Feature";
        case SYSML_KIND_STEP:              return "Step";
        case SYSML_KIND_EXPRESSION:        return "Expression";
        case SYSML_KIND_BOOL_EXPRESSION:   return "BooleanExpression";
        case SYSML_KIND_INVARIANT:         return "Invariant";
        case SYSML_KIND_CONNECTOR:         return "Connector";
        case SYSML_KIND_BINDING_CONNECTOR: return "BindingConnector";
        case SYSML_KIND_SUCCESSION:        return "Succession";
        case SYSML_KIND_KERML_FLOW:        return "Flow";
        case SYSML_KIND_SUCCESSION_FLOW:   return "SuccessionFlow";

        /* Relationships */
        case SYSML_KIND_REL_CONNECTION:  return "ConnectionRelationship";
        case SYSML_KIND_REL_FLOW:        return "FlowRelationship";
        case SYSML_KIND_REL_ALLOCATION:  return "AllocationRelationship";
        case SYSML_KIND_REL_SATISFY:     return "SatisfyRelationship";
        case SYSML_KIND_REL_VERIFY:      return "VerifyRelationship";
        case SYSML_KIND_REL_TRANSITION:  return "TransitionRelationship";
        case SYSML_KIND_REL_SUCCESSION:  return "SuccessionRelationship";
        case SYSML_KIND_REL_BIND:        return "BindRelationship";

        /* KerML Relationships */
        case SYSML_KIND_REL_SPECIALIZATION:    return "SpecializationRelationship";
        case SYSML_KIND_REL_CONJUGATION:       return "ConjugationRelationship";
        case SYSML_KIND_REL_SUBCLASSIFICATION: return "SubclassificationRelationship";
        case SYSML_KIND_REL_DISJOINING:        return "DisjoiningRelationship";
        case SYSML_KIND_REL_INVERTING:         return "FeatureInvertingRelationship";
        case SYSML_KIND_REL_TYPING:            return "FeatureTypingRelationship";
        case SYSML_KIND_REL_SUBSETTING:        return "SubsettingRelationship";
        case SYSML_KIND_REL_REDEFINITION:      return "RedefinitionRelationship";
        case SYSML_KIND_REL_FEATURING:         return "TypeFeaturingRelationship";

        case SYSML_KIND_UNKNOWN:
        default:
            return "Unknown";
    }
}
