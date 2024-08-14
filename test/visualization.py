import pandas as pd
import numpy as np
import matplotlib.pyplot as plt

df = pd.read_csv('performance.csv',
    names=["Dirty", "Normal"], 
    index_col=0
)

df['Ratio(D/N)'] = df['Dirty'] / df['Normal']

df.plot(xlabel='Tabel key size' , ylabel='Time cost(Second)', title='Performance Comparison(10w * keysize)')
plt.show()
