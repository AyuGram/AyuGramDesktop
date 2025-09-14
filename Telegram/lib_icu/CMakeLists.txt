# This is the source code of AyuGram for Desktop.
#
# We do not and cannot prevent the use of our code,
# but be respectful and credit the original author.
#
# Copyright @Radolyn, 2025

add_library(icu_regex_minimal STATIC)

add_library(ayugram::lib_icu ALIAS icu_regex_minimal)

set_property(TARGET icu_regex_minimal PROPERTY CXX_STANDARD 17)
set_property(TARGET icu_regex_minimal PROPERTY CXX_STANDARD_REQUIRED ON)

get_filename_component(src_loc . REALPATH)

nice_target_sources(icu_regex_minimal ${src_loc}
PRIVATE
    icu_source/common/appendable.cpp
    icu_source/common/bmpset.cpp
    icu_source/common/brkeng.cpp
    icu_source/common/brkiter.cpp
    icu_source/common/bytesinkutil.cpp
    icu_source/common/bytestream.cpp
    icu_source/common/bytestrie.cpp
    icu_source/common/bytestriebuilder.cpp
    icu_source/common/bytestrieiterator.cpp
    icu_source/common/caniter.cpp
    icu_source/common/characterproperties.cpp
    icu_source/common/chariter.cpp
    icu_source/common/charstr.cpp
    icu_source/common/cmemory.cpp
    icu_source/common/cstr.cpp
    icu_source/common/cstring.cpp
    icu_source/common/cwchar.cpp
    icu_source/common/dictbe.cpp
    icu_source/common/dictionarydata.cpp
    icu_source/common/dtintrv.cpp
    icu_source/common/edits.cpp
    icu_source/common/emojiprops.cpp
    icu_source/common/errorcode.cpp
    icu_source/common/filteredbrk.cpp
    icu_source/common/filterednormalizer2.cpp
    icu_source/common/icudataver.cpp
    icu_source/common/icuplug.cpp
    icu_source/common/loadednormalizer2impl.cpp
    icu_source/common/localebuilder.cpp
    icu_source/common/localematcher.cpp
    icu_source/common/localeprioritylist.cpp
    icu_source/common/locavailable.cpp
    icu_source/common/locbased.cpp
    icu_source/common/locdispnames.cpp
    icu_source/common/locdistance.cpp
    icu_source/common/locdspnm.cpp
    icu_source/common/locid.cpp
    icu_source/common/loclikely.cpp
    icu_source/common/loclikelysubtags.cpp
    icu_source/common/locmap.cpp
    icu_source/common/locresdata.cpp
    icu_source/common/locutil.cpp
    icu_source/common/lsr.cpp
    icu_source/common/lstmbe.cpp
    icu_source/common/messagepattern.cpp
    icu_source/common/mlbe.cpp
    icu_source/common/normalizer2.cpp
    icu_source/common/normalizer2impl.cpp
    icu_source/common/normlzr.cpp
    icu_source/common/parsepos.cpp
    icu_source/common/patternprops.cpp
    icu_source/common/pluralmap.cpp
    icu_source/common/propname.cpp
    icu_source/common/propsvec.cpp
    icu_source/common/punycode.cpp
    icu_source/common/putil.cpp
    icu_source/common/rbbi.cpp
    icu_source/common/rbbi_cache.cpp
    icu_source/common/rbbidata.cpp
    icu_source/common/rbbinode.cpp
    icu_source/common/rbbirb.cpp
    icu_source/common/rbbiscan.cpp
    icu_source/common/rbbisetb.cpp
    icu_source/common/rbbistbl.cpp
    icu_source/common/rbbitblb.cpp
    icu_source/common/resbund.cpp
    icu_source/common/resbund_cnv.cpp
    icu_source/common/resource.cpp
    icu_source/common/restrace.cpp
    icu_source/common/ruleiter.cpp
    icu_source/common/schriter.cpp
    icu_source/common/serv.cpp
    icu_source/common/servlk.cpp
    icu_source/common/servlkf.cpp
    icu_source/common/servls.cpp
    icu_source/common/servnotf.cpp
    icu_source/common/servrbf.cpp
    icu_source/common/servslkf.cpp
    icu_source/common/sharedobject.cpp
    icu_source/common/simpleformatter.cpp
    icu_source/common/static_unicode_sets.cpp
    icu_source/common/stringpiece.cpp
    icu_source/common/stringtriebuilder.cpp
    icu_source/common/uarrsort.cpp
    icu_source/common/ubidi.cpp
    icu_source/common/ubidi_props.cpp
    icu_source/common/ubidiln.cpp
    icu_source/common/ubiditransform.cpp
    icu_source/common/ubidiwrt.cpp
    icu_source/common/ubrk.cpp
    icu_source/common/ucase.cpp
    icu_source/common/ucasemap.cpp
    icu_source/common/ucasemap_titlecase_brkiter.cpp
    icu_source/common/ucat.cpp
    icu_source/common/uchar.cpp
    icu_source/common/ucharstrie.cpp
    icu_source/common/ucharstriebuilder.cpp
    icu_source/common/ucharstrieiterator.cpp
    icu_source/common/uchriter.cpp
    icu_source/common/ucln_cmn.cpp
    icu_source/common/ucmndata.cpp
    icu_source/common/ucnv.cpp
    icu_source/common/ucnv2022.cpp
    icu_source/common/ucnv_bld.cpp
    icu_source/common/ucnv_cb.cpp
    icu_source/common/ucnv_cnv.cpp
    icu_source/common/ucnv_ct.cpp
    icu_source/common/ucnv_err.cpp
    icu_source/common/ucnv_ext.cpp
    icu_source/common/ucnv_io.cpp
    icu_source/common/ucnv_lmb.cpp
    icu_source/common/ucnv_set.cpp
    icu_source/common/ucnv_u16.cpp
    icu_source/common/ucnv_u32.cpp
    icu_source/common/ucnv_u7.cpp
    icu_source/common/ucnv_u8.cpp
    icu_source/common/ucnvbocu.cpp
    icu_source/common/ucnvdisp.cpp
    icu_source/common/ucnvhz.cpp
    icu_source/common/ucnvisci.cpp
    icu_source/common/ucnvlat1.cpp
    icu_source/common/ucnvmbcs.cpp
    icu_source/common/ucnvscsu.cpp
    icu_source/common/ucnvsel.cpp
    icu_source/common/ucol_swp.cpp
    icu_source/common/ucptrie.cpp
    icu_source/common/ucurr.cpp
    icu_source/common/udata.cpp
    icu_source/common/udatamem.cpp
    icu_source/common/udataswp.cpp
    icu_source/common/uenum.cpp
    icu_source/common/uhash.cpp
    icu_source/common/uhash_us.cpp
    icu_source/common/uidna.cpp
    icu_source/common/uinit.cpp
    icu_source/common/uinvchar.cpp
    icu_source/common/uiter.cpp
    icu_source/common/ulist.cpp
    icu_source/common/uloc.cpp
    icu_source/common/uloc_keytype.cpp
    icu_source/common/uloc_tag.cpp
    icu_source/common/ulocale.cpp
    icu_source/common/ulocbuilder.cpp
    icu_source/common/umapfile.cpp
    icu_source/common/umath.cpp
    icu_source/common/umutablecptrie.cpp
    icu_source/common/umutex.cpp
    icu_source/common/unames.cpp
    icu_source/common/unifiedcache.cpp
    icu_source/common/unifilt.cpp
    icu_source/common/unifunct.cpp
    icu_source/common/uniset.cpp
    icu_source/common/uniset_closure.cpp
    icu_source/common/uniset_props.cpp
    icu_source/common/unisetspan.cpp
    icu_source/common/unistr.cpp
    icu_source/common/unistr_case.cpp
    icu_source/common/unistr_case_locale.cpp
    icu_source/common/unistr_cnv.cpp
    icu_source/common/unistr_props.cpp
    icu_source/common/unistr_titlecase_brkiter.cpp
    icu_source/common/unorm.cpp
    icu_source/common/unormcmp.cpp
    icu_source/common/uobject.cpp
    icu_source/common/uprops.cpp
    icu_source/common/ures_cnv.cpp
    icu_source/common/uresbund.cpp
    icu_source/common/uresdata.cpp
    icu_source/common/usc_impl.cpp
    icu_source/common/uscript.cpp
    icu_source/common/uscript_props.cpp
    icu_source/common/uset.cpp
    icu_source/common/uset_props.cpp
    icu_source/common/usetiter.cpp
    icu_source/common/ushape.cpp
    icu_source/common/usprep.cpp
    icu_source/common/ustack.cpp
    icu_source/common/ustr_cnv.cpp
    icu_source/common/ustr_titlecase_brkiter.cpp
    icu_source/common/ustr_wcs.cpp
    icu_source/common/ustrcase.cpp
    icu_source/common/ustrcase_locale.cpp
    icu_source/common/ustrenum.cpp
    icu_source/common/ustrfmt.cpp
    icu_source/common/ustring.cpp
    icu_source/common/ustrtrns.cpp
    icu_source/common/utext.cpp
    icu_source/common/utf_impl.cpp
    icu_source/common/util.cpp
    icu_source/common/util_props.cpp
    icu_source/common/utrace.cpp
    icu_source/common/utrie.cpp
    icu_source/common/utrie2.cpp
    icu_source/common/utrie2_builder.cpp
    icu_source/common/utrie_swap.cpp
    icu_source/common/uts46.cpp
    icu_source/common/utypes.cpp
    icu_source/common/uvector.cpp
    icu_source/common/uvectr32.cpp
    icu_source/common/uvectr64.cpp
    icu_source/common/wintz.cpp



    icu_source/i18n/unicode/regex.h


    icu_source/i18n/regextxt.h
    icu_source/i18n/regextxt.cpp

    icu_source/i18n/alphaindex.cpp
    icu_source/i18n/anytrans.cpp
    icu_source/i18n/astro.cpp
    icu_source/i18n/basictz.cpp
    icu_source/i18n/bocsu.cpp
    icu_source/i18n/brktrans.cpp
    icu_source/i18n/buddhcal.cpp
    icu_source/i18n/calendar.cpp
    icu_source/i18n/casetrn.cpp
    icu_source/i18n/cecal.cpp
    icu_source/i18n/chnsecal.cpp
    icu_source/i18n/choicfmt.cpp
    icu_source/i18n/coleitr.cpp
    icu_source/i18n/coll.cpp
    icu_source/i18n/collation.cpp
    icu_source/i18n/collationbuilder.cpp
    icu_source/i18n/collationcompare.cpp
    icu_source/i18n/collationdata.cpp
    icu_source/i18n/collationdatabuilder.cpp
    icu_source/i18n/collationdatareader.cpp
    icu_source/i18n/collationdatawriter.cpp
    icu_source/i18n/collationfastlatin.cpp
    icu_source/i18n/collationfastlatinbuilder.cpp
    icu_source/i18n/collationfcd.cpp
    icu_source/i18n/collationiterator.cpp
    icu_source/i18n/collationkeys.cpp
    icu_source/i18n/collationroot.cpp
    icu_source/i18n/collationrootelements.cpp
    icu_source/i18n/collationruleparser.cpp
    icu_source/i18n/collationsets.cpp
    icu_source/i18n/collationsettings.cpp
    icu_source/i18n/collationtailoring.cpp
    icu_source/i18n/collationweights.cpp
    icu_source/i18n/compactdecimalformat.cpp
    icu_source/i18n/coptccal.cpp
    icu_source/i18n/cpdtrans.cpp
    icu_source/i18n/csdetect.cpp
    icu_source/i18n/csmatch.cpp
    icu_source/i18n/csr2022.cpp
    icu_source/i18n/csrecog.cpp
    icu_source/i18n/csrmbcs.cpp
    icu_source/i18n/csrsbcs.cpp
    icu_source/i18n/csrucode.cpp
    icu_source/i18n/csrutf8.cpp
    icu_source/i18n/curramt.cpp
    icu_source/i18n/currfmt.cpp
    icu_source/i18n/currpinf.cpp
    icu_source/i18n/currunit.cpp
    icu_source/i18n/dangical.cpp
    icu_source/i18n/datefmt.cpp
    icu_source/i18n/dayperiodrules.cpp
    icu_source/i18n/dcfmtsym.cpp
    icu_source/i18n/decContext.cpp
    icu_source/i18n/decNumber.cpp
    icu_source/i18n/decimfmt.cpp
    icu_source/i18n/displayoptions.cpp
    icu_source/i18n/double-conversion-bignum-dtoa.cpp
    icu_source/i18n/double-conversion-bignum.cpp
    icu_source/i18n/double-conversion-cached-powers.cpp
    icu_source/i18n/double-conversion-double-to-string.cpp
    icu_source/i18n/double-conversion-fast-dtoa.cpp
    icu_source/i18n/double-conversion-string-to-double.cpp
    icu_source/i18n/double-conversion-strtod.cpp
    icu_source/i18n/dtfmtsym.cpp
    icu_source/i18n/dtitvfmt.cpp
    icu_source/i18n/dtitvinf.cpp
    icu_source/i18n/dtptngen.cpp
    icu_source/i18n/dtrule.cpp
    icu_source/i18n/erarules.cpp
    icu_source/i18n/esctrn.cpp
    icu_source/i18n/ethpccal.cpp
    icu_source/i18n/fmtable.cpp
    icu_source/i18n/fmtable_cnv.cpp
    icu_source/i18n/format.cpp
    icu_source/i18n/formatted_string_builder.cpp
    icu_source/i18n/formattedval_iterimpl.cpp
    icu_source/i18n/formattedval_sbimpl.cpp
    icu_source/i18n/formattedvalue.cpp
    icu_source/i18n/fphdlimp.cpp
    icu_source/i18n/fpositer.cpp
    icu_source/i18n/funcrepl.cpp
    icu_source/i18n/gender.cpp
    icu_source/i18n/gregocal.cpp
    icu_source/i18n/gregoimp.cpp
    icu_source/i18n/hebrwcal.cpp
    icu_source/i18n/indiancal.cpp
    icu_source/i18n/inputext.cpp
    icu_source/i18n/islamcal.cpp
    icu_source/i18n/iso8601cal.cpp
    icu_source/i18n/japancal.cpp
    icu_source/i18n/listformatter.cpp
    icu_source/i18n/measfmt.cpp
    icu_source/i18n/measunit.cpp
    icu_source/i18n/measunit_extra.cpp
    icu_source/i18n/measure.cpp
    icu_source/i18n/msgfmt.cpp
    icu_source/i18n/messageformat2.cpp
    icu_source/i18n/messageformat2_arguments.cpp
    icu_source/i18n/messageformat2_checker.cpp
    icu_source/i18n/messageformat2_data_model.cpp
    icu_source/i18n/messageformat2_errors.cpp
    icu_source/i18n/messageformat2_evaluation.cpp
    icu_source/i18n/messageformat2_formatter.cpp
    icu_source/i18n/messageformat2_formattable.cpp
    icu_source/i18n/messageformat2_function_registry.cpp
    icu_source/i18n/messageformat2_parser.cpp
    icu_source/i18n/messageformat2_serializer.cpp
    icu_source/i18n/name2uni.cpp
    icu_source/i18n/nfrs.cpp
    icu_source/i18n/nfrule.cpp
    icu_source/i18n/nfsubs.cpp
    icu_source/i18n/nortrans.cpp
    icu_source/i18n/nultrans.cpp
    icu_source/i18n/number_affixutils.cpp
    icu_source/i18n/number_asformat.cpp
    icu_source/i18n/number_capi.cpp
    icu_source/i18n/number_compact.cpp
    icu_source/i18n/number_currencysymbols.cpp
    icu_source/i18n/number_decimalquantity.cpp
    icu_source/i18n/number_decimfmtprops.cpp
    icu_source/i18n/number_fluent.cpp
    icu_source/i18n/number_formatimpl.cpp
    icu_source/i18n/number_grouping.cpp
    icu_source/i18n/number_integerwidth.cpp
    icu_source/i18n/number_longnames.cpp
    icu_source/i18n/number_mapper.cpp
    icu_source/i18n/number_modifiers.cpp
    icu_source/i18n/number_multiplier.cpp
    icu_source/i18n/number_notation.cpp
    icu_source/i18n/number_output.cpp
    icu_source/i18n/number_padding.cpp
    icu_source/i18n/number_patternmodifier.cpp
    icu_source/i18n/number_patternstring.cpp
    icu_source/i18n/number_rounding.cpp
    icu_source/i18n/number_scientific.cpp
    icu_source/i18n/number_simple.cpp
    icu_source/i18n/number_skeletons.cpp
    icu_source/i18n/number_symbolswrapper.cpp
    icu_source/i18n/number_usageprefs.cpp
    icu_source/i18n/number_utils.cpp
    icu_source/i18n/numfmt.cpp
    icu_source/i18n/numparse_affixes.cpp
    icu_source/i18n/numparse_compositions.cpp
    icu_source/i18n/numparse_currency.cpp
    icu_source/i18n/numparse_decimal.cpp
    icu_source/i18n/numparse_impl.cpp
    icu_source/i18n/numparse_parsednumber.cpp
    icu_source/i18n/numparse_scientific.cpp
    icu_source/i18n/numparse_symbols.cpp
    icu_source/i18n/numparse_validators.cpp
    icu_source/i18n/numrange_capi.cpp
    icu_source/i18n/numrange_fluent.cpp
    icu_source/i18n/numrange_impl.cpp
    icu_source/i18n/numsys.cpp
    icu_source/i18n/olsontz.cpp
    icu_source/i18n/persncal.cpp
    icu_source/i18n/pluralranges.cpp
    icu_source/i18n/plurfmt.cpp
    icu_source/i18n/plurrule.cpp
    icu_source/i18n/quant.cpp
    icu_source/i18n/quantityformatter.cpp
    icu_source/i18n/rbnf.cpp
    icu_source/i18n/rbt.cpp
    icu_source/i18n/rbt_data.cpp
    icu_source/i18n/rbt_pars.cpp
    icu_source/i18n/rbt_rule.cpp
    icu_source/i18n/rbt_set.cpp
    icu_source/i18n/rbtz.cpp
    icu_source/i18n/regexcmp.cpp
    icu_source/i18n/regeximp.cpp
    icu_source/i18n/regexst.cpp
    icu_source/i18n/regextxt.cpp
    icu_source/i18n/region.cpp
    icu_source/i18n/reldatefmt.cpp
    icu_source/i18n/reldtfmt.cpp
    icu_source/i18n/rematch.cpp
    icu_source/i18n/remtrans.cpp
    icu_source/i18n/repattrn.cpp
    icu_source/i18n/rulebasedcollator.cpp
    icu_source/i18n/scientificnumberformatter.cpp
    icu_source/i18n/scriptset.cpp
    icu_source/i18n/search.cpp
    icu_source/i18n/selfmt.cpp
    icu_source/i18n/sharedbreakiterator.cpp
    icu_source/i18n/simpletz.cpp
    icu_source/i18n/smpdtfmt.cpp
    icu_source/i18n/smpdtfst.cpp
    icu_source/i18n/sortkey.cpp
    icu_source/i18n/standardplural.cpp
    icu_source/i18n/string_segment.cpp
    icu_source/i18n/strmatch.cpp
    icu_source/i18n/strrepl.cpp
    icu_source/i18n/stsearch.cpp
    icu_source/i18n/taiwncal.cpp
    icu_source/i18n/timezone.cpp
    icu_source/i18n/titletrn.cpp
    icu_source/i18n/tmunit.cpp
    icu_source/i18n/tmutamt.cpp
    icu_source/i18n/tmutfmt.cpp
    icu_source/i18n/tolowtrn.cpp
    icu_source/i18n/toupptrn.cpp
    icu_source/i18n/translit.cpp
    icu_source/i18n/transreg.cpp
    icu_source/i18n/tridpars.cpp
    icu_source/i18n/tzfmt.cpp
    icu_source/i18n/tzgnames.cpp
    icu_source/i18n/tznames.cpp
    icu_source/i18n/tznames_impl.cpp
    icu_source/i18n/tzrule.cpp
    icu_source/i18n/tztrans.cpp
    icu_source/i18n/ucal.cpp
    icu_source/i18n/ucln_in.cpp
    icu_source/i18n/ucol.cpp
    icu_source/i18n/ucol_res.cpp
    icu_source/i18n/ucol_sit.cpp
    icu_source/i18n/ucoleitr.cpp
    icu_source/i18n/ucsdet.cpp
    icu_source/i18n/udat.cpp
    icu_source/i18n/udateintervalformat.cpp
    icu_source/i18n/udatpg.cpp
    icu_source/i18n/ufieldpositer.cpp
    icu_source/i18n/uitercollationiterator.cpp
    icu_source/i18n/ulistformatter.cpp
    icu_source/i18n/ulocdata.cpp
    icu_source/i18n/umsg.cpp
    icu_source/i18n/unesctrn.cpp
    icu_source/i18n/uni2name.cpp
    icu_source/i18n/units_data.cpp
    icu_source/i18n/units_complexconverter.cpp
    icu_source/i18n/units_converter.cpp
    icu_source/i18n/units_router.cpp
    icu_source/i18n/unum.cpp
    icu_source/i18n/unumsys.cpp
    icu_source/i18n/upluralrules.cpp
    icu_source/i18n/uregex.cpp
    icu_source/i18n/uregexc.cpp
    icu_source/i18n/uregion.cpp
    icu_source/i18n/usearch.cpp
    icu_source/i18n/uspoof.cpp
    icu_source/i18n/uspoof_build.cpp
    icu_source/i18n/uspoof_conf.cpp
    icu_source/i18n/uspoof_impl.cpp
    icu_source/i18n/utf16collationiterator.cpp
    icu_source/i18n/utf8collationiterator.cpp
    icu_source/i18n/utmscale.cpp
    icu_source/i18n/utrans.cpp
    icu_source/i18n/vtzone.cpp
    icu_source/i18n/vzone.cpp
    icu_source/i18n/windtfmt.cpp
    icu_source/i18n/winnmfmt.cpp
    icu_source/i18n/wintzimpl.cpp
    icu_source/i18n/zonemeta.cpp
    icu_source/i18n/zrule.cpp
    icu_source/i18n/ztrans.cpp

)



target_include_directories(icu_regex_minimal
    PUBLIC
    "${src_loc}/icu_source/common"
    "${src_loc}/icu_source/i18n"
)

function(embed_binary_data TARGET_NAME VAR_NAME INPUT_FILE)
 set(OUTPUT_H "${CMAKE_CURRENT_BINARY_DIR}/generated/${VAR_NAME}.h")
 set(OUTPUT_C "${CMAKE_CURRENT_BINARY_DIR}/generated/${VAR_NAME}.c")


 file(READ "${INPUT_FILE}" file_hex HEX)

 string(REGEX MATCHALL "([0-9a-fA-F]{2})" file_bytes "${file_hex}")
 set(c_array_content "")
 foreach(byte ${file_bytes})
  set(c_array_content "${c_array_content}0x${byte},")
 endforeach()

 file(SIZE "${INPUT_FILE}" file_size)

 set(header_content "
#pragma once
#include <stddef.h>
#ifdef __cplusplus
extern \"C\" {
#endif
extern const unsigned char ${VAR_NAME}[];
extern const size_t ${VAR_NAME}_size;
#ifdef __cplusplus
}
#endif
")
 set(source_content "
#include \"${VAR_NAME}.h\"
const unsigned char ${VAR_NAME}[${file_size}] = { ${c_array_content} };
const size_t ${VAR_NAME}_size = ${file_size};
")

 file(WRITE "${OUTPUT_H}" "${header_content}")
 file(WRITE "${OUTPUT_C}" "${source_content}")

 target_sources(${TARGET_NAME} PRIVATE "${OUTPUT_C}")
 target_include_directories(${TARGET_NAME} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
endfunction()

set(MINIMAL_ICU_DAT_FILE "${src_loc}/data/icudt78l.dat")
embed_binary_data(icu_regex_minimal icudt78_dat "${MINIMAL_ICU_DAT_FILE}")


target_compile_definitions(icu_regex_minimal PRIVATE
    U_STATIC_IMPLEMENTATION=1
    U_COMMON_IMPLEMENTATION=1
    U_I18N_IMPLEMENTATION=1


    # as we build it static, we need to remove
    # __declspec(dllimport) and __declspec(dllexport)
    U_EXPORT=
    U_COMMON_API=
    U_I18N_API=

    UCONFIG_NO_COLLATION=1
    UCONFIG_NO_FORMATTING=1
    UCONFIG_NO_TRANSLITERATION=1
    UCONFIG_NO_BREAK_ITERATION=1
    UCONFIG_NO_LEGACY_CONVERSION=1
    UCONFIG_NO_REGULAR_EXPRESSIONS=0 # to be sure
)
if(MSVC)
 # otherwise "too many characters in constant" will occur in uchar.cpp
 target_compile_options(icu_regex_minimal PRIVATE "/utf-8")
endif()
