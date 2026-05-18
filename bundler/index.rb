require "bundler"

module Bundler
  class Index
    def initialize
      @specs = []
    end

    def <<(spec)
      @specs << spec
      self
    end

    def size
      @specs.length
    end

    def empty?
      @specs.empty?
    end

    def names
      @specs.map(&:name)
    end

    def dependency_names
      names.uniq.sort
    end

    def search(dependency)
      @specs.select do |spec|
        next false unless spec.name == dependency.name
        dependency.requirement.satisfied_by?(Gem::Version.new(spec.version))
      end
    end

    def [](name)
      @specs.select { |spec| spec.name == name.to_s }
    end

    def each(&block)
      @specs.each(&block)
    end

    def include?(specification)
      needle =
        if specification.respond_to?(:full_name)
          specification.full_name
        else
          specification.to_s
        end
      @specs.any? { |spec| spec.full_name == needle }
    end

    def use(other)
      other.each { |spec| self << spec }
      self
    end
  end
end
