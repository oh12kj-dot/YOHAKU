"""Pure geometry normalization shared by generation and its regression checks."""
def clip_collision_parts(parts, dimensions):
    low = [-dimensions[0]/2, -dimensions[1]/2, 0]
    high = [dimensions[0]/2, dimensions[1]/2, dimensions[2]]
    result = []
    for part in parts:
        a = [max(low[i], part['center'][i]-part['size'][i]/2) for i in range(3)]
        b = [min(high[i], part['center'][i]+part['size'][i]/2) for i in range(3)]
        if all(b[i]-a[i] > .001 for i in range(3)):
            result.append({'center':[round((a[i]+b[i])/2, 4) for i in range(3)],
                           'size':[round(b[i]-a[i], 4) for i in range(3)]})
    return result
