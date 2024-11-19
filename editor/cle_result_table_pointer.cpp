#include <QMenu>
#include <QScrollBar>

#include "cle_result_table_pointer.h"
#include "cle_common.h"

CleResultTablePointer::CleResultTablePointer(QWidget *parent, uint32_t address,
   uint8_t size, uint8_t passes, uint32_t range, uint32_t max_results)
{
   char offset_str[16];
   uint8_t i;

   CleResultTable::init();

   /* Pointer-specific table styling */
   m_Table->setColumnCount(3 + passes);

   /* Initialize result table column headers */
   QStringList TableHeader;
   TableHeader << tr("Initial");
   for (i = 0; i < passes; i++)
   {
      snprintf(offset_str, sizeof(offset_str), "Offset %u", i + 1);
      TableHeader << offset_str;
   }
   TableHeader << tr("Previous") << tr("Current");
   m_Table->setHorizontalHeaderLabels(TableHeader);

   m_ColAddress   = 0;
   m_ColValuePrev = passes + 1;
   m_ColValueCurr = passes + 2;

   /* Qt connections to parent */
   connect(this, SIGNAL(addressChanged(cl_addr_t)),
      parent, SLOT(onAddressChanged(cl_addr_t)));
   connect(this, SIGNAL(requestAddMemoryNote(cl_memnote_t)),
      parent, SLOT(requestAddMemoryNote(cl_memnote_t)));
   connect(this, SIGNAL(requestPointerSearch(cl_addr_t)),
      parent, SLOT(requestPointerSearch(cl_addr_t)));

   cl_pointersearch_init(&m_Search, address, size, passes, range, max_results);
   rebuild();
}

CleResultTablePointer::~CleResultTablePointer()
{
   cl_pointersearch_free(&m_Search);
}

cl_addr_t CleResultTablePointer::getClickedResultAddress()
{
   return m_Search.results[m_Table->currentRow()].address_final;
}

void* CleResultTablePointer::getSearchData()
{
   return (void*)(&m_Search);
}

void CleResultTablePointer::onClickResultAddMemoryNote()
{
   cl_memnote_t note;

   note.address         = m_Search.results[m_ClickedResult].address_initial;
   note.type            = m_Search.params.value_type;
   note.pointer_offsets = m_Search.results[m_ClickedResult].offsets;
   note.pointer_passes  = m_Search.passes;

   emit requestAddMemoryNote(note);
}

void CleResultTablePointer::onResultRightClick(const QPoint& pos)
{
   if (pos.isNull())
      return;
   else
   {
      m_ClickedResult = m_Table->rowAt(pos.y());
      if (m_ClickedResult < 0 || m_ClickedResult >= m_Table->rowCount())
         return;
      else
      {
         QMenu menu;
         QAction *action_add    = menu.addAction(tr("&Add memory note..."));

         connect(action_add, SIGNAL(triggered()), this, 
            SLOT(onClickResultAddMemoryNote()));

         menu.exec(m_Table->mapToGlobal(pos));
      }
   }
}

/* TODO: Allow seeking to multiple steps */
void CleResultTablePointer::onResultClick(QTableWidgetItem *item)
{
   if (!item)
      return;
   else
   {
      if (item->column() == 0)
         emit addressChanged(m_Search.results[item->row()].address_initial);
      else
         emit addressChanged(m_Search.results[item->row()].address_final);
   }
}

void CleResultTablePointer::onResultDoubleClick()
{
   if (m_Table->currentColumn() == m_ColValueCurr)
   {
      uint32_t i;

      /* We gray out the other entries because they won't update while
         we're editing. */
      for (i = 0; i < m_Table->rowCount(); i++)
         m_Table->item(i, m_ColValueCurr)->setTextColor(Qt::gray);
      m_CurrentEditedRow = m_Table->currentRow();
   }
}

void CleResultTablePointer::onResultEdited(QTableWidgetItem *item)
{
   if (item->row() == m_CurrentEditedRow && item->column() == m_ColValueCurr)
   {
      if (item->isSelected())
      {
         writeMemory(m_Search.results[item->row()].address_final, 
            m_Search.params, item->text());
      }
      m_CurrentEditedRow = -1;
   }
}

void CleResultTablePointer::rebuild()
{
   char     temp_string[32];
   uint8_t  size;
   uint32_t current_row, temp_value, i, j;

   size = m_Search.params.value_type;
   m_Table->setColumnCount(3 + m_Search.passes);
   m_Table->setRowCount(0);

   for (i = 0; i < m_Search.result_count; i++)
   {
      m_Table->insertRow(i);

      snprintf(temp_string, 256, "%08X", m_Search.results[i].address_initial);
      m_Table->setItem(i, m_ColAddress, new QTableWidgetItem(QString(temp_string)));

      for (j = 0; j < m_Search.passes; j++)
      {
         snprintf(temp_string, 256, "%02X", m_Search.results[i].offsets[j]);
         m_Table->setItem(i, j + 1, new QTableWidgetItem(QString(temp_string)));
      }

      valueToString(temp_string, sizeof(temp_string), m_Search.results[i].value_previous, size);
      m_Table->setItem(i, m_ColValuePrev, new QTableWidgetItem(QString(temp_string)));

      valueToString(temp_string, sizeof(temp_string), m_Search.results[i].value_current, size);
      m_Table->setItem(i, m_ColValueCurr, new QTableWidgetItem(QString(temp_string)));
   }
}

void CleResultTablePointer::reset(void)
{
   cl_pointersearch_free(&m_Search);
}

void CleResultTablePointer::run()
{
   QTableWidgetItem *item;
   char     temp_string[32];
   uint8_t  val_type;
   uint32_t address, value_curr, value_prev, i;

   val_type = m_Search.params.value_type;

   /* The C code updates all of the pointer results */
   cl_pointersearch_update(&m_Search);

   for (i = 0; i < m_Search.result_count; i++)
   {
      item = m_Table->item(i, 0);
      if (!item)
         return;

      /* Don't visually update search results that are out of view */
      if (i < m_Table->verticalScrollBar()->value())
         continue;
      else if (i > m_Table->verticalScrollBar()->value() 
          + m_Table->size().height() / 16)
         break;

      value_curr = m_Search.results[i].value_current;
      value_prev = m_Search.results[i].value_previous;

      /* Update previous value column */
      item = m_Table->item(i, m_ColValuePrev);
      valueToString(temp_string, sizeof(temp_string), value_prev, val_type);
      item->setText(temp_string);

      /* Current value column */
      if (m_CurrentEditedRow < 0)
      {
         item = m_Table->item(i, m_ColValueCurr);

         valueToString(temp_string, sizeof(temp_string), value_curr, val_type);
         item->setText(temp_string);

         /* Display changed values in red */
         item->setTextColor(value_prev != value_curr ? Qt::red : Qt::white);
      }
   }
}

bool CleResultTablePointer::step(const QString& text)
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
      cl_pointersearch_step
      (
         &m_Search,
         no_input ? NULL : compare_value
      );
   else
      return false;

   free(compare_value);
   rebuild();

   return true;
}
