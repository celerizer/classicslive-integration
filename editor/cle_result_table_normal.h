#ifndef CLE_RESULT_TABLE_NORMAL_H
#define CLE_RESULT_TABLE_NORMAL_H

#include <QWidget>

extern "C"
{
   #include "../cl_memory.h"
   #include "../cl_search.h"
}

#include "cle_result_table.h"

/* TODO: Move this to a config option? */
#define CLE_SEARCH_MAX_ROWS 1000

class CleResultTableNormal : public CleResultTable
{
   Q_OBJECT

public:
   CleResultTableNormal(QWidget* parent);
   ~CleResultTableNormal();

   uint32_t getClickedResultAddress() override;
   void* getSearchData() override;
   bool isInitted() { return true; }
   void rebuild() override;
   void reset(uint8_t value_type) override;
   void run() override;
   bool step(const QString& text) override;

   uint8_t getCompareType() { return m_Search.params.compare_type; }
   uint8_t getValueType() { return m_Search.params.value_type; }

   void setCompareType(const uint8_t new_type) { m_Search.params.compare_type = new_type; }
   void setValueType(const uint8_t new_type) { m_Search.params.value_type = new_type; m_Search.params.size = cl_sizeof_memtype(new_type); }

public slots:
   void onClickResultAddMemoryNote();
   void onClickResultPointerSearch();
   void onClickResultRemove();
   void onResultClick(QTableWidgetItem *item) override;
   void onResultDoubleClick(void) override;
   void onResultEdited(QTableWidgetItem *item) override;
   void onResultRightClick(const QPoint&) override;
   void onResultSelectionChanged(void) override;

signals:
   void addressChanged(uint32_t address);
   void requestAddMemoryNote(cl_memnote_t note);
   void requestPointerSearch(uint32_t address);
   //void requestRemove(uint32_t index) override;

private:
   cl_search_t m_Search;
};

#endif
