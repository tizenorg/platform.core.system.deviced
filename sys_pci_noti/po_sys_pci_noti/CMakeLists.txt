# for i18n

SET(POFILES ar.po el_GR.po es_ES.po fi.po gl.po hy.po ka.po lv.po pl.po ru_RU.po sv.po zh_CN.po az.po cs.po en_PH.po es_MX.po fr_CA.po hi.po is.po kk.po mk.po pt_BR.po sk.po tr_TR.po zh_HK.po bg.po da.po en.po et.po fr_FR.po hr.po it_IT.po ko_KR.po nb.po pt_PT.po sl.po uk.po zh_SG.po ca.po de_DE.po en_US.po eu.po ga.po hu.po ja_JP.po lt.po nl_NL.po ro.po sr.po uz.po zh_TW.po)

SET(MSGFMT "/usr/bin/msgfmt")

FOREACH(pofile ${POFILES})
	SET(pofile ${CMAKE_CURRENT_SOURCE_DIR}/${pofile})
	MESSAGE("PO: ${pofile}")
	GET_FILENAME_COMPONENT(absPofile ${pofile} ABSOLUTE)
	GET_FILENAME_COMPONENT(lang ${absPofile} NAME_WE)
	SET(moFile ${CMAKE_CURRENT_BINARY_DIR}/${lang}.mo)
	ADD_CUSTOM_COMMAND(
			OUTPUT ${moFile}
			COMMAND ${MSGFMT} -o ${moFile} ${absPofile}
			DEPENDS ${absPofile}
	)
	INSTALL(FILES ${moFile}
			DESTINATION share/system-server/sys_pci_noti/res/locale/${lang}/LC_MESSAGES RENAME ${PROJECT_NAME}.mo)
	SET(moFiles ${moFiles} ${moFile})
ENDFOREACH(pofile)

MESSAGE(".mo files: ${moFiles}")
ADD_CUSTOM_TARGET(po_sys_pci_noti ALL DEPENDS ${moFiles})
