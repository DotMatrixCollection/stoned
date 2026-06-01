class CvParent
  @@value = 1
  def self.value; @@value; end
end

class CvChild < CvParent
  @@value = 2
end

p CvParent.value
p CvChild.value
