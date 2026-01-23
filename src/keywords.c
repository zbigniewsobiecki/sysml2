/*
 * SysML v2 Parser - Keyword Recognition
 *
 * Uses a hash table for O(1) keyword lookup.
 *
 * SPDX-License-Identifier: MIT
 */

#include "sysml2/lexer.h"
#include <string.h>

/* Keyword entry */
typedef struct {
    const char *name;
    Sysml2TokenType type;
} KeywordEntry;

/* All KerML and SysML v2 keywords */
static const KeywordEntry keywords[] = {
    /* KerML Core Keywords */
    {"about", SYSML2_TOKEN_KW_ABOUT},
    {"abstract", SYSML2_TOKEN_KW_ABSTRACT},
    {"alias", SYSML2_TOKEN_KW_ALIAS},
    {"all", SYSML2_TOKEN_KW_ALL},
    {"and", SYSML2_TOKEN_KW_AND},
    {"as", SYSML2_TOKEN_KW_AS},
    {"assoc", SYSML2_TOKEN_KW_ASSOC},
    {"behavior", SYSML2_TOKEN_KW_BEHAVIOR},
    {"binding", SYSML2_TOKEN_KW_BINDING},
    {"bool", SYSML2_TOKEN_KW_BOOL},
    {"by", SYSML2_TOKEN_KW_BY},
    {"chains", SYSML2_TOKEN_KW_CHAINS},
    {"class", SYSML2_TOKEN_KW_CLASS},
    {"classifier", SYSML2_TOKEN_KW_CLASSIFIER},
    {"comment", SYSML2_TOKEN_KW_COMMENT},
    {"composite", SYSML2_TOKEN_KW_COMPOSITE},
    {"conjugate", SYSML2_TOKEN_KW_CONJUGATE},
    {"conjugates", SYSML2_TOKEN_KW_CONJUGATES},
    {"conjugation", SYSML2_TOKEN_KW_CONJUGATION},
    {"connector", SYSML2_TOKEN_KW_CONNECTOR},
    {"datatype", SYSML2_TOKEN_KW_DATATYPE},
    {"default", SYSML2_TOKEN_KW_DEFAULT},
    {"derived", SYSML2_TOKEN_KW_DERIVED},
    {"differences", SYSML2_TOKEN_KW_DIFFERENCES},
    {"disjoining", SYSML2_TOKEN_KW_DISJOINING},
    {"disjoint", SYSML2_TOKEN_KW_DISJOINT},
    {"doc", SYSML2_TOKEN_KW_DOC},
    {"else", SYSML2_TOKEN_KW_ELSE},
    {"end", SYSML2_TOKEN_KW_END},
    {"expr", SYSML2_TOKEN_KW_EXPR},
    {"false", SYSML2_TOKEN_KW_FALSE},
    {"feature", SYSML2_TOKEN_KW_FEATURE},
    {"featured", SYSML2_TOKEN_KW_FEATURED},
    {"featuring", SYSML2_TOKEN_KW_FEATURING},
    {"filter", SYSML2_TOKEN_KW_FILTER},
    {"first", SYSML2_TOKEN_KW_FIRST},
    {"from", SYSML2_TOKEN_KW_FROM},
    {"function", SYSML2_TOKEN_KW_FUNCTION},
    {"hastype", SYSML2_TOKEN_KW_HASTYPE},
    {"if", SYSML2_TOKEN_KW_IF},
    {"implies", SYSML2_TOKEN_KW_IMPLIES},
    {"import", SYSML2_TOKEN_KW_IMPORT},
    {"in", SYSML2_TOKEN_KW_IN},
    {"inout", SYSML2_TOKEN_KW_INOUT},
    {"interaction", SYSML2_TOKEN_KW_INTERACTION},
    {"intersects", SYSML2_TOKEN_KW_INTERSECTS},
    {"intersecting", SYSML2_TOKEN_KW_INTERSECTING},
    {"inv", SYSML2_TOKEN_KW_INV},
    {"inverse", SYSML2_TOKEN_KW_INVERSE},
    {"istype", SYSML2_TOKEN_KW_ISTYPE},
    {"language", SYSML2_TOKEN_KW_LANGUAGE},
    {"library", SYSML2_TOKEN_KW_LIBRARY},
    {"locale", SYSML2_TOKEN_KW_LOCALE},
    {"member", SYSML2_TOKEN_KW_MEMBER},
    {"metaclass", SYSML2_TOKEN_KW_METACLASS},
    {"metadata", SYSML2_TOKEN_KW_METADATA},
    {"multiplicity", SYSML2_TOKEN_KW_MULTIPLICITY},
    {"namespace", SYSML2_TOKEN_KW_NAMESPACE},
    {"nonunique", SYSML2_TOKEN_KW_NONUNIQUE},
    {"not", SYSML2_TOKEN_KW_NOT},
    {"null", SYSML2_TOKEN_KW_NULL},
    {"of", SYSML2_TOKEN_KW_OF},
    {"or", SYSML2_TOKEN_KW_OR},
    {"ordered", SYSML2_TOKEN_KW_ORDERED},
    {"out", SYSML2_TOKEN_KW_OUT},
    {"package", SYSML2_TOKEN_KW_PACKAGE},
    {"portion", SYSML2_TOKEN_KW_PORTION},
    {"predicate", SYSML2_TOKEN_KW_PREDICATE},
    {"private", SYSML2_TOKEN_KW_PRIVATE},
    {"protected", SYSML2_TOKEN_KW_PROTECTED},
    {"public", SYSML2_TOKEN_KW_PUBLIC},
    {"readonly", SYSML2_TOKEN_KW_READONLY},
    {"redefines", SYSML2_TOKEN_KW_REDEFINES},
    {"redefinition", SYSML2_TOKEN_KW_REDEFINITION},
    {"ref", SYSML2_TOKEN_KW_REF},
    {"references", SYSML2_TOKEN_KW_REFERENCES},
    {"rep", SYSML2_TOKEN_KW_REP},
    {"return", SYSML2_TOKEN_KW_RETURN},
    {"specialization", SYSML2_TOKEN_KW_SPECIALIZATION},
    {"specializes", SYSML2_TOKEN_KW_SPECIALIZES},
    {"step", SYSML2_TOKEN_KW_STEP},
    {"struct", SYSML2_TOKEN_KW_STRUCT},
    {"subclassifier", SYSML2_TOKEN_KW_SUBCLASSIFIER},
    {"subset", SYSML2_TOKEN_KW_SUBSET},
    {"subsets", SYSML2_TOKEN_KW_SUBSETS},
    {"subtype", SYSML2_TOKEN_KW_SUBTYPE},
    {"succession", SYSML2_TOKEN_KW_SUCCESSION},
    {"then", SYSML2_TOKEN_KW_THEN},
    {"to", SYSML2_TOKEN_KW_TO},
    {"true", SYSML2_TOKEN_KW_TRUE},
    {"type", SYSML2_TOKEN_KW_TYPE},
    {"typed", SYSML2_TOKEN_KW_TYPED},
    {"typing", SYSML2_TOKEN_KW_TYPING},
    {"unions", SYSML2_TOKEN_KW_UNIONS},
    {"unioning", SYSML2_TOKEN_KW_UNIONING},
    {"xor", SYSML2_TOKEN_KW_XOR},
    {"loop", SYSML2_TOKEN_KW_LOOP},

    /* SysML v2 Keywords */
    {"accept", SYSML2_TOKEN_KW_ACCEPT},
    {"action", SYSML2_TOKEN_KW_ACTION},
    {"actor", SYSML2_TOKEN_KW_ACTOR},
    {"after", SYSML2_TOKEN_KW_AFTER},
    {"allocation", SYSML2_TOKEN_KW_ALLOCATION},
    {"analysis", SYSML2_TOKEN_KW_ANALYSIS},
    {"assert", SYSML2_TOKEN_KW_ASSERT},
    {"assign", SYSML2_TOKEN_KW_ASSIGN},
    {"assumption", SYSML2_TOKEN_KW_ASSUMPTION},
    {"at", SYSML2_TOKEN_KW_AT},
    {"attribute", SYSML2_TOKEN_KW_ATTRIBUTE},
    {"calc", SYSML2_TOKEN_KW_CALC},
    {"case", SYSML2_TOKEN_KW_CASE},
    {"concern", SYSML2_TOKEN_KW_CONCERN},
    {"connection", SYSML2_TOKEN_KW_CONNECTION},
    {"constraint", SYSML2_TOKEN_KW_CONSTRAINT},
    {"decide", SYSML2_TOKEN_KW_DECIDE},
    {"def", SYSML2_TOKEN_KW_DEF},
    {"dependency", SYSML2_TOKEN_KW_DEPENDENCY},
    {"do", SYSML2_TOKEN_KW_DO},
    {"entry", SYSML2_TOKEN_KW_ENTRY},
    {"enum", SYSML2_TOKEN_KW_ENUM},
    {"event", SYSML2_TOKEN_KW_EVENT},
    {"exhibit", SYSML2_TOKEN_KW_EXHIBIT},
    {"exit", SYSML2_TOKEN_KW_EXIT},
    {"expose", SYSML2_TOKEN_KW_EXPOSE},
    {"flow", SYSML2_TOKEN_KW_FLOW},
    {"for", SYSML2_TOKEN_KW_FOR},
    {"fork", SYSML2_TOKEN_KW_FORK},
    {"frame", SYSML2_TOKEN_KW_FRAME},
    {"include", SYSML2_TOKEN_KW_INCLUDE},
    {"individual", SYSML2_TOKEN_KW_INDIVIDUAL},
    {"interface", SYSML2_TOKEN_KW_INTERFACE},
    {"item", SYSML2_TOKEN_KW_ITEM},
    {"join", SYSML2_TOKEN_KW_JOIN},
    {"merge", SYSML2_TOKEN_KW_MERGE},
    {"message", SYSML2_TOKEN_KW_MESSAGE},
    {"objective", SYSML2_TOKEN_KW_OBJECTIVE},
    {"occurrence", SYSML2_TOKEN_KW_OCCURRENCE},
    {"parallel", SYSML2_TOKEN_KW_PARALLEL},
    {"part", SYSML2_TOKEN_KW_PART},
    {"perform", SYSML2_TOKEN_KW_PERFORM},
    {"port", SYSML2_TOKEN_KW_PORT},
    {"receive", SYSML2_TOKEN_KW_RECEIVE},
    {"rendering", SYSML2_TOKEN_KW_RENDERING},
    {"req", SYSML2_TOKEN_KW_REQ},
    {"require", SYSML2_TOKEN_KW_REQUIRE},
    {"requirement", SYSML2_TOKEN_KW_REQUIREMENT},
    {"satisfy", SYSML2_TOKEN_KW_SATISFY},
    {"send", SYSML2_TOKEN_KW_SEND},
    {"snapshot", SYSML2_TOKEN_KW_SNAPSHOT},
    {"stakeholder", SYSML2_TOKEN_KW_STAKEHOLDER},
    {"state", SYSML2_TOKEN_KW_STATE},
    {"subject", SYSML2_TOKEN_KW_SUBJECT},
    {"timeslice", SYSML2_TOKEN_KW_TIMESLICE},
    {"transition", SYSML2_TOKEN_KW_TRANSITION},
    {"use", SYSML2_TOKEN_KW_USE},
    {"variant", SYSML2_TOKEN_KW_VARIANT},
    {"verification", SYSML2_TOKEN_KW_VERIFICATION},
    {"verify", SYSML2_TOKEN_KW_VERIFY},
    {"via", SYSML2_TOKEN_KW_VIA},
    {"view", SYSML2_TOKEN_KW_VIEW},
    {"viewpoint", SYSML2_TOKEN_KW_VIEWPOINT},
    {"when", SYSML2_TOKEN_KW_WHEN},
    {"while", SYSML2_TOKEN_KW_WHILE},
};

#define KEYWORD_COUNT (sizeof(keywords) / sizeof(keywords[0]))

/* Hash table size (should be prime and larger than KEYWORD_COUNT * 2) */
#define HASH_TABLE_SIZE 353

/* Hash table bucket */
typedef struct {
    const char *key;
    size_t length;
    Sysml2TokenType type;
} HashEntry;

static HashEntry hash_table[HASH_TABLE_SIZE];
static bool keywords_initialized = false;

/* FNV-1a hash */
static uint32_t hash_string(const char *str, size_t length) {
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < length; i++) {
        hash ^= (uint8_t)str[i];
        hash *= 16777619u;
    }
    return hash;
}

void sysml2_keywords_init(void) {
    if (keywords_initialized) return;

    /* Clear table */
    memset(hash_table, 0, sizeof(hash_table));

    /* Insert all keywords */
    for (size_t i = 0; i < KEYWORD_COUNT; i++) {
        const char *key = keywords[i].name;
        size_t length = strlen(key);
        uint32_t hash = hash_string(key, length);
        size_t index = hash % HASH_TABLE_SIZE;

        /* Linear probing for collision resolution */
        while (hash_table[index].key != NULL) {
            index = (index + 1) % HASH_TABLE_SIZE;
        }

        hash_table[index].key = key;
        hash_table[index].length = length;
        hash_table[index].type = keywords[i].type;
    }

    keywords_initialized = true;
}

Sysml2TokenType sysml2_keyword_lookup(const char *str, size_t length) {
    if (!keywords_initialized) {
        sysml2_keywords_init();
    }

    uint32_t hash = hash_string(str, length);
    size_t index = hash % HASH_TABLE_SIZE;

    /* Linear probing */
    while (hash_table[index].key != NULL) {
        if (hash_table[index].length == length &&
            memcmp(hash_table[index].key, str, length) == 0) {
            return hash_table[index].type;
        }
        index = (index + 1) % HASH_TABLE_SIZE;
    }

    return SYSML2_TOKEN_IDENTIFIER;
}
