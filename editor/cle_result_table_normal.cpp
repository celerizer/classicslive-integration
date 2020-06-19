#ifndef CLE_RESULT_TABLE_NORMAL_CPP
#define CLE_RESULT_TABLE_NORMAL_CPP

#include <QScrollBar>
#include <QStringList>

#include "cle_result_table_normal.h"
#include "cle_common.h"

#define COL_ADDRESS        0
#define COL_PREVIOUS_VALUE 1
#define COL_CURRENT_VALUE  2

CleResultTableNormal::CleResultTableNormal()
{
   CleResultTable::init();

   /* Normal-specific table styling */
   m_Table->setColumnCount(3);

   /* Initialize result table column headers */
   QStringList TableHeader;
   TableHeader << tr("Address") << tr("Previous") << tr("Current");
   m_Table->setHorizontalHeaderLabels(TableHeader);

   cl_search_init(&m_Search);
}

CleResultTableNormal::~CleResultTableNormal()
{
   cl_search_free(&m_Search);
}

uint32_t CleResultTableNormal::getClickedResultAddress()
{
   return m_Table->item(m_Table->currentRow(), COL_ADDRESS)->text().split(" ")[0].toULong(NULL, 16);
}

void *CleResultTableNormal::getSearchData()
{
   return (void*)(&m_Search);
}

void CleResultTableNormal::onResultClick()
{
   emit addressChanged(getClickedResultAddress() & ~0xF);
}

void CleResultTableNormal::onResultDoubleClick()
{
   if (m_Table->currentColumn() == COL_CURRENT_VALUE)
   {
      uint32_t i;

      /* We gray out the other entries because they won't update while
         we're editing. */
      for (i = 0; i < m_Table->rowCount(); i++)
         m_Table->item(i, COL_CURRENT_VALUE)->setTextColor(Qt::gray);
      m_CurrentEditedRow = m_Table->currentRow();
   }
}

void CleResultTableNormal::onResultEdited(QTableWidgetItem *result)
{
   if (result->row() == m_CurrentEditedRow && result->column() == COL_CURRENT_VALUE)
   {
      if (result->isSelected())
      {
         uint32_t  address;
         QString   new_value_text;
         bool      ok = true;
         void     *value;

         address = getClickedResultAddress();
         new_value_text = m_Table->item(m_CurrentEditedRow, COL_CURRENT_VALUE)->text();

         if (m_Search.params.value_type == CL_MEMTYPE_FLOAT)
            value = new float(new_value_text.toFloat(&ok));
         else
            value = new uint32_t(stringToValue(new_value_text, &ok));

         if (ok)
         {
            cl_write_memory(NULL, address, m_Search.params.size, value);
            cl_log("Wrote %s to 0x%08X.\n", new_value_text.toStdString().c_str(), address);
         }
         free(value);
      }
      m_CurrentEditedRow = -1;
   }
}

void CleResultTableNormal::onResultSelectionChanged()
{
   m_CurrentEditedRow = -1;
}

void CleResultTableNormal::rebuild()
{
   char     temp_string[32];
   uint8_t  size, val_type;
   uint32_t current_row, matches, temp_value, i, j;

   /* (De)allocate rows */
   matches = m_Search.matches;
   if (matches > CLE_SEARCH_MAX_ROWS)
      matches = CLE_SEARCH_MAX_ROWS;
   else if (matches == 0)
   {
      m_Table->setRowCount(0);
      return;
   }

   current_row = 0;
   size        = m_Search.params.size;
   val_type    = m_Search.params.value_type;

   for (i = 0; i < memory.bank_count; i++)
   {
      if (!m_Search.searchbanks[i].any_valid)
         continue;

      for (j = 0; j < memory.banks[i].size; j += size)
      {
         /* This value was filtered out */
         if (!m_Search.searchbanks[i].valid[j])
            continue;

         /* This value is still valid; add a new row */
         m_Table->insertRow(current_row);

         /* Address */
         snprintf(temp_string, 256, "%08X", j + memory.banks[i].start);
         m_Table->setItem(current_row, 0, new QTableWidgetItem(QString(temp_string)));
         /* Previous value */
         cl_read_search(&temp_value, &m_Search, &m_Search.searchbanks[i], j);
         valueToString(temp_string, sizeof(temp_string), temp_value, val_type);
         m_Table->setItem(current_row, 1, new QTableWidgetItem(QString(temp_string)));
         /* Current value */
         cl_read_memory(&temp_value, &memory.banks[i], j, size);
         valueToString(temp_string, sizeof(temp_string), temp_value, val_type);
         m_Table->setItem(current_row, 2, new QTableWidgetItem(QString(temp_string)));
         current_row++;

         /* No need to continue */
         if (current_row == matches)
         {
            m_Table->setRowCount(matches);
            return;
         }
      }
   }
}

void CleResultTableNormal::reset(uint8_t value_type)
{
   cl_search_reset(&m_Search);
   m_Search.params.value_type = value_type;
   m_Table->setRowCount(0);
}

void CleResultTableNormal::run()
{
   QTableWidgetItem *item;
   char     temp_string[32];
   uint8_t  size, val_type;
   uint32_t address, curr_value, prev_value, i, j;

   size     = m_Search.params.size;
   val_type = m_Search.params.value_type;

   for (i = 0; i < m_Table->rowCount(); i++)
   {
      item = m_Table->item(i, 0);
      if (!item)
         return;

      /* Don't visually update search results that are out of view */
      if (i < m_Table->verticalScrollBar()->value())
         continue;
      else if (i > m_Table->verticalScrollBar()->value() + m_Table->size().height() / 16)
         break;

      /* Kind of gross, but should save some memory 
         Only a few results should be redrawn at any time anyway */
      address = item->text().split(" ")[0].toULong(NULL, 16);

      if (!cl_read_memory(&curr_value, NULL, address, size) ||
          !cl_read_search(&prev_value, &m_Search, NULL, address))
         break;

      /* Update previous value column */
      item = m_Table->item(i, COL_PREVIOUS_VALUE);
      valueToString(temp_string, sizeof(temp_string), prev_value, val_type);
      item->setText(temp_string);

      /* Update current value column */
      if (m_CurrentEditedRow < 0)
      {
         item = m_Table->item(i, COL_CURRENT_VALUE);

         valueToString(temp_string, sizeof(temp_string), curr_value, val_type);
         item->setText(temp_string);

         /* Display changed values in red */
         item->setTextColor(prev_value != curr_value ? Qt::red : Qt::white);
      }
   }
}

bool CleResultTableNormal::step(const QString& text)
{
   void *compare_value;
   bool  no_input = text.isEmpty();
   bool  ok = true;

   if (m_Search.params.value_type == CL_MEMTYPE_FLOAT)
      compare_value = new float(text.toFloat(&ok));
   else
      compare_value = new uint32_t(stringToValue(text, &ok));

   /* Run the C code for doing the actual search */
   if (ok || no_input)
      cl_search_step
      (
         &m_Search,
         no_input ? NULL : compare_value
      );
   else if (text.front() == '"' && text.back() == '"')
      cl_search_ascii
      (
         &m_Search, 
         text.mid(1, text.length() - 2).toStdString().c_str(), 
         text.length() - 2
      );
   else
      return false;
   free(compare_value);
   rebuild();

   return true;
}

#endif
