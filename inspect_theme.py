try:
    import qdarktheme
    print("qdarktheme imported successfully")
    print(f"Attributes: {dir(qdarktheme)}")
except ImportError:
    print("qdarktheme not found")
except Exception as e:
    print(f"Error: {e}")
