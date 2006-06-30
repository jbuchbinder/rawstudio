/*
 * RAWstudio - Rawstudio is an open source raw-image converter written in GTK+.
 * by Anders BRander <anders@brander.dk> and Anders Kvist <akv@lnxbx.dk>
 *
 * rs-cache.c - cache interface
 *
 * Rawstudio is licensed under the GNU General Public License.
 * It uses DCRaw and UFraw code to do the actual raw decoding.
 */

#include <gtk/gtk.h>
#include <libxml/encoding.h>
#include <libxml/xmlwriter.h>
#include "matrix.h"
#include "rawstudio.h"

gchar *
rs_cache_get_name(const gchar *src)
{
	gchar *ret=NULL;
	gchar *dotdir, *filename;
	GString *out;
	dotdir = rs_dotdir_get(src);
	filename = g_path_get_basename(src);
	if (dotdir)
	{
		out = g_string_new(dotdir);
		out = g_string_append(out, "/");
		out = g_string_append(out, filename);
		out = g_string_append(out, ".cache.xml");
		ret = out->str;
		g_string_free(out, FALSE);
		g_free(dotdir);
	}
	g_free(filename);
	return(ret);
}

void
rs_cache_init()
{
	static gboolean init=FALSE;
	if (!init)
		LIBXML_TEST_VERSION /* yep, it should look like this */
	init = TRUE;
	return;
}

void
rs_cache_save(RS_PHOTO *photo)
{
	gint id;
	xmlTextWriterPtr writer;
	gchar *cachename;

	if(!photo->active) return;
	cachename = rs_cache_get_name(photo->filename);
	if (!cachename) return;
	writer = xmlNewTextWriterFilename(cachename, 0); /* fixme, check for errors */
	xmlTextWriterStartDocument(writer, NULL, "ISO-8859-1", NULL);
	xmlTextWriterStartElement(writer, BAD_CAST "rawstudio-cache");
	xmlTextWriterWriteFormatElement(writer, BAD_CAST "priority", "%d",
		photo->priority);
	xmlTextWriterWriteFormatElement(writer, BAD_CAST "orientation", "%d",
		photo->orientation);
	for(id=0;id<3;id++)
	{
		xmlTextWriterStartElement(writer, BAD_CAST "settings");
		xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "id", "%d", id);
		xmlTextWriterWriteFormatElement(writer, BAD_CAST "exposure", "%f",
			GETVAL(photo->settings[id]->exposure));
		xmlTextWriterWriteFormatElement(writer, BAD_CAST "saturation", "%f",
			GETVAL(photo->settings[id]->saturation));
		xmlTextWriterWriteFormatElement(writer, BAD_CAST "hue", "%f",
			GETVAL(photo->settings[id]->hue));
		xmlTextWriterWriteFormatElement(writer, BAD_CAST "contrast", "%f",
			GETVAL(photo->settings[id]->contrast));
		xmlTextWriterWriteFormatElement(writer, BAD_CAST "warmth", "%f",
			GETVAL(photo->settings[id]->warmth));
		xmlTextWriterWriteFormatElement(writer, BAD_CAST "tint", "%f",
			GETVAL(photo->settings[id]->tint));
		xmlTextWriterEndElement(writer);
	}
	xmlTextWriterEndDocument(writer);
	xmlFreeTextWriter(writer);
	g_free(cachename);
	return;
}

void
rs_cache_load_setting(RS_SETTINGS *rss, xmlDocPtr doc, xmlNodePtr cur)
{
	xmlChar *val;
	GtkObject *target=NULL;
	while(cur)
	{
		target = NULL;
		if ((!xmlStrcmp(cur->name, BAD_CAST "exposure")))
			target = rss->exposure;
		else if ((!xmlStrcmp(cur->name, BAD_CAST "saturation")))
			target = rss->saturation;
		else if ((!xmlStrcmp(cur->name, BAD_CAST "hue")))
			target = rss->hue;
		else if ((!xmlStrcmp(cur->name, BAD_CAST "contrast")))
			target = rss->contrast;
		else if ((!xmlStrcmp(cur->name, BAD_CAST "warmth")))
			target = rss->warmth;
		else if ((!xmlStrcmp(cur->name, BAD_CAST "tint")))
			target = rss->tint;

		if (target)
		{
			val = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			SETVAL(target, g_strtod((gchar *) val, NULL));
			xmlFree(val);
		}
		cur = cur->next;
	}
}

void
rs_cache_load(RS_PHOTO *photo)
{
	xmlDocPtr doc;
	xmlNodePtr cur;
	xmlChar *val;
	gchar *cachename;
	gint id;

	cachename = rs_cache_get_name(photo->filename);
	if (!cachename) return;
	if (!g_file_test(cachename, G_FILE_TEST_IS_REGULAR)) return;
	doc = xmlParseFile(cachename);
	if(doc==NULL) return;

	cur = xmlDocGetRootElement(doc);

	cur = cur->xmlChildrenNode;
	while(cur)
	{
		if ((!xmlStrcmp(cur->name, BAD_CAST "settings")))
		{
			val = xmlGetProp(cur, BAD_CAST "id");
			id = atoi((gchar *) val);
			xmlFree(val);
			if (id>2) id=0;
			if (id<0) id=0;
			rs_cache_load_setting(photo->settings[id], doc, cur->xmlChildrenNode);
		}
		else if ((!xmlStrcmp(cur->name, BAD_CAST "priority")))
		{
			val = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			photo->priority = atoi((gchar *) val);
			xmlFree(val);
		}
		else if ((!xmlStrcmp(cur->name, BAD_CAST "orientation")))
		{
			val = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			photo->orientation = atoi((gchar *) val);
			xmlFree(val);
		}
		cur = cur->next;
	}
	
	xmlFreeDoc(doc);
	g_free(cachename);
	return;
}

void
rs_cache_load_quick(const gchar *filename, gint *priority)
{
	xmlDocPtr doc;
	xmlNodePtr cur;
	xmlChar *val;
	gchar *cachename;

	cachename = rs_cache_get_name(filename);
	if (!cachename) return;
	if (!g_file_test(cachename, G_FILE_TEST_IS_REGULAR)) return;
	doc = xmlParseFile(cachename);
	if(doc==NULL) return;

	cur = xmlDocGetRootElement(doc);

	cur = cur->xmlChildrenNode;
	while(cur)
	{
		if ((!xmlStrcmp(cur->name, BAD_CAST "priority")))
		{
			val = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			*priority = atoi((gchar *) val);
			xmlFree(val);
		}
		cur = cur->next;
	}
	
	xmlFreeDoc(doc);
	g_free(cachename);
	return;
}
