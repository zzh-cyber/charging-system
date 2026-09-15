import json, shutil, sys
from datetime import datetime, timezone
from pathlib import Path
from pyspark.sql import SparkSession, functions as F
from pyspark.sql.window import Window
from pyspark.ml.feature import VectorAssembler
from pyspark.ml.regression import RandomForestRegressor
from pyspark.ml.evaluation import RegressionEvaluator

ROOT=Path(__file__).resolve().parent.parent
TRAIN=ROOT/'bigdata/work/ads/train/train.csv'; DWS=ROOT/'bigdata/work/dws/station_hour/station_hour.csv'; OUT=ROOT/'bigdata/work/evaluation'
FEATURE_COLS=['hour_of_day','weekday','is_weekend','is_holiday','lag1_kwh','mean_24h_kwh','horizon','kwh','idle_piles']; PARAMS={'numTrees':16,'maxDepth':6,'seed':20260913}; HS=(1,6,24)

def cast(df, cols):
    for c,t in cols: df=df.withColumn(c,F.col(c).cast(t))
    return df
def metric(df,label,pred,smape=False):
    out={m.upper():RegressionEvaluator(labelCol=label,predictionCol=pred,metricName=m).evaluate(df) for m in ('mae','rmse','r2')}
    if smape: out['sMAPE']=df.select(F.avg(F.when((F.abs(F.col(label))+F.abs(F.col(pred)))==0,0).otherwise(2*F.abs(F.col(pred)-F.col(label))/(F.abs(F.col(label))+F.abs(F.col(pred))))*100).alias('x')).first()['x']
    return out
def main():
    missing=[str(p) for p in (TRAIN,DWS) if not p.is_file()]
    if missing: print('评估输入缺失，拒绝回退到 mini 数据：\n'+'\n'.join('  '+p for p in missing)); return 2
    spark=SparkSession.builder.master('local[*]').appName('rf-model-evaluation').getOrCreate()
    try:
        tr=cast(spark.read.option('header',True).csv(TRAIN.as_uri()),[(c,'double') for c in ('kwh','idle_piles','lag1_kwh','mean_24h_kwh','label_kwh','label_idle')]+[(c,'int') for c in ('hour_of_day','weekday','is_weekend','is_holiday','horizon')]+[('station_id','long'),('hour_ts','timestamp')])
        d=cast(spark.read.option('header',True).csv(DWS.as_uri()),[(c,t) for c,t in (('station_id','long'),('hour_ts','timestamp'),('kwh','double'),('idle_piles','double'),('total_piles','double'),('hour_of_day','int'),('weekday','int'),('is_weekend','int'),('is_holiday','int'))])
        input_rows=tr.count(); exact=[]
        for h in HS:
            o=tr.filter(F.col('horizon')==h).alias('o')
            x=o.join(d.alias('t'),(F.col('o.station_id')==F.col('t.station_id'))&(F.col('t.hour_ts')==F.col('o.hour_ts')+F.expr(f'INTERVAL {h} HOURS')),'inner').select('o.station_id',F.col('o.hour_ts').alias('origin_time'),F.col('t.hour_ts').alias('target_time'),F.lit(h).alias('horizon'),*[F.col('o.'+c).alias(c) for c in FEATURE_COLS if c!='horizon'],F.col('t.kwh').alias('label_kwh'),F.col('t.idle_piles').alias('label_idle'))
            exact.append(x)
        x=exact[0].unionByName(exact[1]).unionByName(exact[2]).na.drop(subset=FEATURE_COLS+['label_kwh','label_idle']); usable=x.count(); cutoff=x.select(F.expr('percentile_approx(origin_time,0.8)').alias('c')).first()['c']; train=x.filter((F.col('target_time')<F.lit(cutoff))&(F.col('origin_time')<F.lit(cutoff))); test=x.filter(F.col('origin_time')>=F.lit(cutoff)); train_rows=train.count(); test_rows=test.count(); purged_rows=usable-train_rows-test_rows; print('input_rows=',input_rows,'usable_rows=',usable,'dropped_rows=',input_rows-usable,'purged_rows=',purged_rows,'cutoff_time=',cutoff,'train_rows=',train_rows,'test_rows=',test_rows)
        a=VectorAssembler(inputCols=FEATURE_COLS,outputCol='features'); km={}; im={}; bm={}; fi={}; ps=[]
        for h in HS:
            trh=a.transform(train.filter(F.col('horizon')==h)); teh=a.transform(test.filter(F.col('horizon')==h)); mk=RandomForestRegressor(featuresCol='features',labelCol='label_kwh',predictionCol='pred_kwh',**PARAMS).fit(trh); mi=RandomForestRegressor(featuresCol='features',labelCol='label_idle',predictionCol='pred_idle',**PARAMS).fit(trh); p=mi.transform(mk.transform(teh)).withColumn('gap_hours',(F.col('target_time').cast('long')-F.col('origin_time').cast('long'))/3600); km[f'{h}h']=metric(p,'label_kwh','pred_kwh',True); im[f'{h}h']=metric(p,'label_idle','pred_idle'); fi[f'{h}h']={'kwh':list(mk.featureImportances),'idle':list(mi.featureImportances)}; bm[f'{h}h']={'kwh_RMSE':RegressionEvaluator(labelCol='label_kwh',predictionCol='kwh',metricName='rmse').evaluate(p),'idle_RMSE':RegressionEvaluator(labelCol='label_idle',predictionCol='idle_piles',metricName='rmse').evaluate(p)}; ps.append(p.select('station_id','origin_time','target_time','gap_hours','horizon','kwh','idle_piles','label_kwh','pred_kwh','label_idle','pred_idle'));
        allp=ps[0].unionByName(ps[1]).unionByName(ps[2]); OUT.mkdir(parents=True,exist_ok=True); tmp=OUT/'_predictions'; shutil.rmtree(tmp,ignore_errors=True); allp.coalesce(1).write.mode('overwrite').option('header',True).csv(tmp.as_uri()); shutil.copy2(next(tmp.glob('part-*.csv')),OUT/'predictions.csv'); gap={}
        for h in HS:
            r=allp.filter(F.col('horizon')==h).agg(F.count('*').alias('n'),F.avg('gap_hours').alias('mean'),F.expr('percentile_approx(gap_hours,0.5)').alias('median'),F.sum(F.when(F.col('gap_hours')==h,1).otherwise(0)).alias('exact')).first(); gap[f'{h}h']={'samples':r.n,'mean_gap_hours':r['mean'],'median_gap_hours':r['median'],'exact_gap_samples':r['exact'],'exact_gap_rate':r['exact']/r.n if r.n else 0.0}
        payload={'generated_at':datetime.now(timezone.utc).astimezone().strftime('%Y-%m-%d %H:%M:%S'),'split_method':'purged_time_split_target_before_cutoff_origin_test','cutoff_time':str(cutoff),'input_rows':input_rows,'usable_rows':usable,'dropped_rows':input_rows-usable,'purged_rows':purged_rows,'train_rows':train_rows,'test_rows':test_rows,'feature_cols':FEATURE_COLS,'model_params':PARAMS,'gap_diagnostics_scope':'test','gap_diagnostics':gap,'kwh_metrics':km,'idle_metrics':im,'baseline_metrics':bm,'feature_importances':fi}; (OUT/'model_metrics.json').write_text(json.dumps(payload,ensure_ascii=False,indent=2),encoding='utf-8')
        print('horizon target MAE RMSE R2 sMAPE baseline_RMSE improvement');
        for target,vals,bkey in (('kwh',km,'kwh_RMSE'),('idle',im,'idle_RMSE')):
            for h in ('1h','6h','24h'): m=vals[h]; b=bm[h][bkey]; print(h,target,round(m['MAE'],4),round(m['RMSE'],4),round(m['R2'],4),round(m['sMAPE'],4) if 'sMAPE' in m else 'N/A',round(b,4),round((b-m['RMSE'])*100/b,2) if b else 0)
    finally: spark.stop()
if __name__=='__main__': sys.exit(main())
